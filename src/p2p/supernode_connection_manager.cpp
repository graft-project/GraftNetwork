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
//  if (req.supernode_id.empty()) {
//    MERROR("Failed to register supernode: empty id");
//    return;
//  }
//  MDEBUG("registering supernode: " << req.supernode_id << ", url: " << req.supernode_url);
  
//  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  
//  SupernodeConnection& sn = m_supernode_connections[req.supernode_id];
//  sn.redirect_uri = req.redirect_uri;
//  sn.redirect_timeout_ms = req.redirect_timeout_ms;
//  sn.expiry_time = get_expiry_time(req.supernode_id);
  
//  {//set sn.client & sn.uri
//    epee::net_utils::http::url_content parsed{};
//    bool ret = epee::net_utils::parse_url(req.supernode_url, parsed);
//    sn.uri = std::move(parsed.uri);
//    if (sn.client.is_connected()) 
//      sn.client.disconnect();
//    sn.client.set_server(parsed.host, std::to_string(parsed.port), {});
//  }
  add_rta_route(req.supernode_id, req.supernode_url);
}
// TODO:
void SupernodeConnectionManager::add_rta_route(const std::string &id, const std::string &url)
{
  MDEBUG("adding/updating route for '" << id << " via '" << url << "'");
  
  if (id.empty()) {
    MERROR("Failed to register supernode: empty id");
    return;
  }
  MDEBUG("registering supernode: " << id << ", url: " << url);
  
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  
  std::string full_url = url; // FIXME
  SupernodeConnection& sn = m_supernode_connections[id];
  
  // TODO:
  //  sn.redirect_uri = req.redirect_uri;
  //  sn.redirect_timeout_ms = req.redirect_timeout_ms;
  //  sn.expiry_time = get_expiry_time(req.supernode_id);
  
  {//set sn.client & sn.uri
    epee::net_utils::http::url_content parsed{};
    bool ret = epee::net_utils::parse_url(full_url, parsed);
    if (!ret) {
      MERROR("Failed to parse url: " << full_url);
      return;
    }
    sn.uri = std::move(parsed.uri);
    if (sn.client.is_connected()) 
      sn.client.disconnect();
    sn.client.set_server(parsed.host, std::to_string(parsed.port), {});
  }
}

void SupernodeConnectionManager::remove_expired_routes()
{
//  std::set<std::string> known_supernodes;
//  {   
//    for (auto it = m_supernode_routes.begin(); it != m_supernode_routes.end(); ++it)
//    {
//      SupernodeRoutes& routes = it->second;
//      auto now = Clock::now();
      
//      // erase expired records
//      routes.erase(std::remove_if(routes.begin(), routes.end(), [now](SupernodeRoute& v)->bool{ return v.expiry_time < now; } ), routes.end());
      
//      //erase empty redirector
//      if(routes.empty())
//      {
//        it = m_supernode_routes.erase(it);
//        continue;
//      }
//      known_supernodes.insert(it->first);
//      // to_sns.emplace_back(recs[0].it_local_sn->first); //choose only the first one
//    }
//  }
}

std::vector<SupernodeConnectionManager::SupernodeId> SupernodeConnectionManager::connections() const
{
  std::vector<std::string> result;
  std::for_each(m_supernode_connections.begin(), m_supernode_connections.end(), 
                [&result](decltype(*m_supernode_connections.begin())& pair){ result.push_back(pair.first); });
  return result;
}

bool SupernodeConnectionManager::processBroadcast(typename nodetool::COMMAND_BROADCAST::request &arg, bool &relay_broadcast, uint64_t &messages_sent)
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
  
  
  
  std::vector<std::string> known_addresses, unknown_addresses;
  
  for (const std::string &address : arg.receiver_addresses) {
    MDEBUG("checking destination address: " << address);
    if (m_supernode_connections.find(address) != m_supernode_connections.end()) {
      known_addresses.push_back(address); // destination is a local supernode
      MDEBUG("destination address is a local supernode");
    } else {
      unknown_addresses.push_back(address);
      MDEBUG("destination address is a unknown supernode");
    }
  }
  // in case 'receiver_addresses' is empty -> broadcast to all - post to all 
  if (arg.receiver_addresses.empty()) {
    MDEBUG("==> broadcast to all, sending to local supernode(s)");
    known_addresses = all_local_supernodes;
  }
  
  {//MDEBUG
    std::ostringstream oss;
    oss << "\nknown_addresses: ";
    for(auto& it : known_addresses) { oss << it << "\n"; }
    oss << "\nknown_supernodes: ";
    for(auto& it : unknown_addresses) { oss << it << "\n"; }
    MDEBUG("==> broadcast\n" << oss.str());
  }
  if (!known_addresses.empty())
  {
    MDEBUG("handle_broadcast: posting to supernodes: " << "arg { receiver_addresses : '" << boost::algorithm::join(arg.receiver_addresses, " ") << "'\n arg.callback_uri : '" << arg.callback_uri << "'}");
    MDEBUG("known_addresses " << boost::algorithm::join(known_addresses, "\n"));
#ifdef UDHT_INFO
    arg.hops = arg.hop;
#endif
    for (const auto& id : known_addresses)
    {
      // XXX: what is "broadcast_to_me" ? A: is is JSON-RPC method which is unused on supernode side, 
      // only endpoint specified in 'arg.callback_uri' used
      m_supernode_connections[id].callJsonRpc<nodetool::COMMAND_BROADCAST>("" /* pass to local supernode */, arg, arg.callback_uri);
      ++messages_sent;
      MDEBUG("called local supernode: " << id);
    }
  }

  relay_broadcast = !unknown_addresses.empty();
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
  return false;
}

std::string SupernodeConnectionManager::dump_routes() const
{
 
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
