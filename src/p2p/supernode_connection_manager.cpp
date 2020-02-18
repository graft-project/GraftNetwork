#include "supernode_connection_manager.h"
#include "storages/http_abstract_invoke.h"

#include <boost/algorithm/string/join.hpp>

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "net.p2p.supernode"

namespace graft {


bool SupernodeConnectionManager::SupernodeConnection::operator==(const SupernodeConnection &other) const
{
  return this->client.get_host() == other.client.get_host()
      && this->client.get_port() == other.client.get_port()
      && this->uri == other.uri
      && this->redirect_uri == other.redirect_uri;
}

SupernodeConnectionManager::SupernodeConnectionManager()
{
  
}

SupernodeConnectionManager::~SupernodeConnectionManager()
{
  
}

void SupernodeConnectionManager::register_supernode(const cryptonote::COMMAND_RPC_REGISTER_SUPERNODE::request &req)
{
  if (req.supernode_id.empty()) {
    MERROR("Failed to register supernode: empty id");
    return;
  }
  MDEBUG("registering supernode: " << req.supernode_id << ", url: " << req.supernode_url);
  
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  
  SupernodeConnection& sn = m_supernode_connections[req.supernode_id];
  sn.redirect_uri = req.redirect_uri;
  sn.redirect_timeout_ms = req.redirect_timeout_ms;
  sn.expiry_time = get_expiry_time(req.supernode_id);
  
  {//set sn.client & sn.uri
    epee::net_utils::http::url_content parsed{};
    bool ret = epee::net_utils::parse_url(req.supernode_url, parsed);
    sn.uri = std::move(parsed.uri);
    if (sn.client.is_connected()) 
      sn.client.disconnect();
    sn.client.set_server(parsed.host, std::to_string(parsed.port), {});
  }
}

void SupernodeConnectionManager::add_rta_route(const std::string &id, const std::string &my_id)
{
  MDEBUG("adding/updating route for '" << id << " via '" << my_id << "'");
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  auto connection = m_supernode_connections.find(my_id);
  if (connection == m_supernode_connections.end()) {
    MERROR("Failed to add route: " << my_id << " is unknown supernode"); 
    return;
  }
  auto expiry_time = get_expiry_time(my_id);
  bool route_exists = m_supernode_routes.find(id) != m_supernode_routes.end();
  SupernodeRoutes& routes = m_supernode_routes[id];
  if (route_exists) {
    MDEBUG("route to '" << id <<  "' via '" << my_id << "'  exists, updating");
  } else {
    MDEBUG("route to '" << id <<  "' via '" << my_id << "' doesn't exist, adding");
  }
  
  auto route = std::find_if(routes.begin(), routes.end(), [connection](const SupernodeRoute& r)->bool { return r.supernode_ptr == connection; });
  if (route == routes.end())
  {
    routes.emplace_back(SupernodeRoute{connection, expiry_time});
  }
  else
  {
    assert(route->supernode_ptr == connection);
    route->expiry_time = expiry_time;
  }
  
}

void SupernodeConnectionManager::remove_expired_routes()
{
  // TODO
}

std::vector<SupernodeConnectionManager::SupernodeId> SupernodeConnectionManager::connections() const
{
  std::vector<std::string> result;
  std::for_each(m_supernode_connections.begin(), m_supernode_connections.end(), 
                [&result](decltype(*m_supernode_connections.begin())& pair){ result.push_back(pair.first); });
  return result;
}

bool SupernodeConnectionManager::processBroadcast(typename nodetool::COMMAND_BROADCAST::request &arg, bool &relay_broadcast, uint64_t &messages_sent, uint64_t &messages_forwarded)
{
  MDEBUG("processBroadcast: begin");
  std::vector<std::string> all_local_supernodes;
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  {
    //prepare sorted sns
    std::for_each(m_supernode_connections.begin(), m_supernode_connections.end(), [&all_local_supernodes](decltype(*m_supernode_connections.begin())& pair){ all_local_supernodes.push_back(pair.first); });
    MDEBUG("processBroadcast: sender_address: " << arg.sender_address
           << ", local supernodes: " << boost::algorithm::join(all_local_supernodes, ", "));
    MDEBUG("processBroadcast: receiver_addresses: " << "receiver addresses: " << boost::algorithm::join(arg.receiver_addresses, ", "));
  }
  
  std::set<std::string> known_supernodes;
  {   
    for (auto it = m_supernode_routes.begin(); it != m_supernode_routes.end(); ++it)
    {
      SupernodeRoutes& routes = it->second;
      auto now = Clock::now();
      
      // erase expired records
      routes.erase(std::remove_if(routes.begin(), routes.end(), [now](SupernodeRoute& v)->bool{ return v.expiry_time < now; } ), routes.end());
      
      //erase empty redirector
      if(routes.empty())
      {
        it = m_supernode_routes.erase(it);
        continue;
      }
      known_supernodes.insert(it->first);
      // to_sns.emplace_back(recs[0].it_local_sn->first); //choose only the first one
    }
  }
  
  std::vector<std::string> local_addresses, known_addresses, unknown_addresses;
  
  for (const std::string &address : arg.receiver_addresses) {
    MDEBUG("checking destination address: " << address);
    if (m_supernode_connections.find(address) != m_supernode_connections.end()) {
      local_addresses.push_back(address); // destination is a local supernode
      MDEBUG("destination address is a local supernode");
    } else if (std::find(known_supernodes.begin(), known_supernodes.end(), address) != known_supernodes.end()) {
      known_addresses.push_back(address);
      MDEBUG("destination address is a known supernode");
    } else {
      unknown_addresses.push_back(address);
      MDEBUG("destination address is a unknown supernode");
    }
  }
  // in case 'receiver_addresses' is empty -> broadcast to all - post to all 
  if (arg.receiver_addresses.empty()) {
    MDEBUG("==> broadcast to all, sending to local supernode(s)");
    local_addresses = all_local_supernodes;
  }
  
  {//MDEBUG
    std::ostringstream oss;
    oss << "local_addresses: ";
    for(auto& it : local_addresses) { oss << it << "\n"; }
    oss << "\nknown_addresses: ";
    for(auto& it : known_addresses) { oss << it << "\n"; }
//    oss << "\nknown_supernodes: ";
//    for(auto& it : known_supernodes) { oss << it << "\n"; }
    oss << "\nunknown_addresses: ";
    for(auto& it : unknown_addresses) { oss << it << "\n"; }
    MDEBUG("==> broadcast\n" << oss.str());
  }
  if (!local_addresses.empty())
  {
    MDEBUG("handle_broadcast: posting to local supernodes: " << "arg { receiver_addresses : '" << boost::algorithm::join(arg.receiver_addresses, " ") << "'\n arg.callback_uri : '" << arg.callback_uri << "'}");
    MDEBUG("local_addresses " << boost::algorithm::join(local_addresses, "\n"));
#ifdef UDHT_INFO
    arg.hops = arg.hop;
#endif
    for (const auto& id : local_addresses)
    {
      // XXX: what is "broadcast_to_me" ? A: is is JSON-RPC method which is unused on supernode side, 
      // only endpoint specified in 'arg.callback_uri' used
      m_supernode_connections[id].callJsonRpc<nodetool::COMMAND_BROADCAST>("" /* pass to local supernode */, arg, arg.callback_uri);
      ++messages_sent;
      MDEBUG("called local supernode: " << id);
    }
  }
  // TODO: Q: What is the difference in known_addresses vs local_addresses  and why they processed in separate loops?
  //       A: Difference is the messages to local and known supernodes are delivered via different RPC 
  if (/*!known_addresses.empty()*/false) // XXX: disable UDHT for now, only using p2p
  {// forward to local supernode which knows destination network address
    // TODO: IK not sure if it needed at all - why it needs to be encapsulated into different message
    MDEBUG("handle_broadcast: posting to known supernodes: " << "arg { receiver_addresses : '" << boost::algorithm::join(arg.receiver_addresses, "\n") << "'\n arg.callback_uri : '" << arg.callback_uri << "'}");
    MDEBUG("known_addresses " << boost::algorithm::join(known_addresses, " "));

    cryptonote::COMMAND_RPC_REDIRECT_BROADCAST::request redirect_req;
    redirect_req.request.receiver_addresses = arg.receiver_addresses; 
    redirect_req.request.sender_address = arg.sender_address;
    redirect_req.request.callback_uri = arg.callback_uri;
#ifdef UDHT_INFO
    redirect_req.request.hop =  arg.hops;
#endif
    redirect_req.request.data = arg.data;
    redirect_req.request.signature = arg.signature;
    
    
// #if 0  // Do not send redirect broadcast for now        
    // TODO: non-optimal way to process known_addresses. as one local supernode might know more than one remote supernode
    for (auto& id : known_addresses)
    {
      auto it = m_supernode_routes.find(id);
      assert(it != m_supernode_routes.end());
      assert(!it->second.empty());
      SupernodeRoute& rec = it->second[0];
      SupernodeConnection& sn = rec.supernode_ptr->second;
      std::string callback_url = sn.redirect_uri;
      MDEBUG("==> redirect broadcast for " << id << " > " << sn.client.get_host() << ":" << sn.client.get_port()  << " url =" << callback_url);
      redirect_req.receiver_id = id;
      // 2nd argument means 'method' in JSON-RPC but supernode doesn't use JSON-RPC but REST instead, so it's simply ignored on supernode side
      sn.callJsonRpc<cryptonote::COMMAND_RPC_REDIRECT_BROADCAST>("" /*forward to another supernode via local supernode*/, redirect_req, callback_url);
      ++messages_forwarded;
    }
// #endif           
  }
  // XXX disable UDHT for now
  std::copy(known_addresses.begin(), known_addresses.end(), std::back_inserter(unknown_addresses));
  
  relay_broadcast = true;
  // modify original destinations - exclude local and known addresses we already processes
  if (!arg.receiver_addresses.empty())
  {
    if(unknown_addresses.empty())
    {
      relay_broadcast = false; // do not relay in case all addresses already processed
    }
    else
    {
      arg.receiver_addresses.clear();
      std::copy(unknown_addresses.begin(), unknown_addresses.end(), std::back_inserter(arg.receiver_addresses));
    }
  }
  MDEBUG("processBroadcast: end");
  return true;
}

bool SupernodeConnectionManager::has_connections() const
{
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  return !m_supernode_connections.empty();
}

bool SupernodeConnectionManager::has_routes() const
{
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  return !m_supernode_routes.empty();
}

std::string SupernodeConnectionManager::dump_routes() const
{
  std::ostringstream oss;
  for (const auto & route : m_supernode_routes) {
    oss << "destination: " << route.first << " can reached via following nodes:\n";
    oss << "\t\t";
    for (const auto &destination : route.second) {
      oss << destination.supernode_ptr->first << " ";
    }
    oss << "\n";
  }
  return oss.str();
}

std::string SupernodeConnectionManager::dump_connections() const
{
  std::ostringstream oss;
  for (const auto & conn : m_supernode_connections) {
    oss << "id: " << conn.first << " is " << conn.second.client.get_host() << ":" << conn.second.client.get_port() << "\n";
  }
  return oss.str();
}

SupernodeConnectionManager::Clock::time_point 
SupernodeConnectionManager::get_expiry_time(const SupernodeConnectionManager::SupernodeId &local_sn)
{
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  auto it = m_supernode_connections.find(local_sn);
  assert(it != m_supernode_connections.end());
  return SupernodeConnectionManager::Clock::now() + std::chrono::milliseconds(it->second.redirect_timeout_ms);
}

} // namespace graft
