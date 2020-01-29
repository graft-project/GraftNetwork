#include "supernode_connection_manager.h"
#include "storages/http_abstract_invoke.h"

#include <boost/algorithm/string/join.hpp>

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "net.p2p.supernode"

namespace graft {

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
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  auto it = m_supernode_connections.find(my_id);
  if (it == m_supernode_connections.end()) {
    MERROR("Failed to add route: " << my_id << " is unknown supernode"); 
    return;
  }
  auto expiry_time = get_expiry_time(my_id);
  SupernodeRoutes& recs = m_supernode_routes[id];
  auto it2 = std::find_if(recs.begin(), recs.end(), [it](const SupernodeRoute& r)->bool { return r.supernode_ptr == it; });
  if (it2 == recs.end())
  {
    recs.emplace_back(SupernodeRoute{it, expiry_time});
  }
  else
  {
    assert(it2->supernode_ptr == it);
    it2->expiry_time = expiry_time;
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
  MDEBUG("P2P Request: handle_broadcast: lock");
  std::vector<std::string> local_sns;
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  {
    //prepare sorted sns
    
    std::for_each(m_supernode_connections.begin(), m_supernode_connections.end(), [&local_sns](decltype(*m_supernode_connections.begin())& pair){ local_sns.push_back(pair.first); });
    MDEBUG("P2P Request: handle_broadcast: sender_address: " << arg.sender_address
           << ", local supernodes: " << boost::algorithm::join(local_sns, ", "));
    MDEBUG("P2P Request: handle_broadcast: receiver_addresses: " << "receiver addresses: " << boost::algorithm::join(arg.receiver_addresses, ", "));
  }
  
  std::vector<std::string> redirect_sns; // TODO: the purpose of this? 
  {   
    redirect_sns.reserve(m_supernode_routes.size()); //not exact, but
    // to_sns.reserve(m_supernode_routes.size()); //not exact, but
    for (auto it = m_supernode_routes.begin(); it != m_supernode_routes.end(); ++it)
    {
      SupernodeRoutes& recs = it->second;
      auto now = Clock::now();
      
      //erase dead records
      recs.erase(std::remove_if(recs.begin(), recs.end(), [now](SupernodeRoute& v)->bool{ return v.expiry_time < now; } ), recs.end());
      
      //erase empty redirector
      if(recs.empty())
      {
        it = m_supernode_routes.erase(it);
        continue;
      }
      redirect_sns.emplace_back(it->first);
      // to_sns.emplace_back(recs[0].it_local_sn->first); //choose only the first one
    }
  }
  
  std::vector<std::string> local_addresses, known_addresses, unknown_addresses;
  
  for (const std::string &address : arg.receiver_addresses) {
    if (m_supernode_connections.find(address) != m_supernode_connections.end()) 
      local_addresses.push_back(address);
    else if (std::find(redirect_sns.begin(), redirect_sns.end(), address) != redirect_sns.end())
      known_addresses.push_back(address);
    else 
      unknown_addresses.push_back(address);
  }
  // in case 'receiver_addresses' is empty -> broadcast to all - post to all 
  if (arg.receiver_addresses.empty()) {
    MDEBUG("==> broadcast to all, sending to local supernode(s)");
    local_addresses = local_sns;
  }
  
  {//MDEBUG
    std::ostringstream oss;
    oss << "local_addresses\n";
    for(auto& it : local_addresses) { oss << it << "\n"; }
    oss << "known_addresses\n";
    for(auto& it : known_addresses) { oss << it << "\n"; }
    oss << "unknown_addresses\n";
    for(auto& it : unknown_addresses) { oss << it << "\n"; }
    MDEBUG("==> broadcast\n") << oss.str();
  }
  if (!local_addresses.empty())
  {
    MDEBUG("handle_broadcast: posting to local supernodes: ") << "arg { receiver_addresses : '" << boost::algorithm::join(arg.receiver_addresses, " ") << "'\n arg.callback_uri : '" << arg.callback_uri << "'}";
    MDEBUG("local_addresses ") << boost::algorithm::join(local_addresses, "\n");
#ifdef UDHT_INFO
    arg.hops = arg.hop;
#endif
    for (const auto& id : local_addresses)
    {
      // XXX: what is "broadcast_to_me" ? A: is is JSON-RPC method which is unused on supernode side, 
      // only endpoint specified in 'arg.callback_uri' used
      
      m_supernode_connections[id].callJsonRpc<cryptonote::COMMAND_RPC_BROADCAST>("" /* pass to local supernode */, arg, arg.callback_uri);
      ++messages_sent;
    }
  }
  // TODO: What is the difference in known_addresses vs local_addresses  and why they processed in separate loops?
  if (!known_addresses.empty())
  {//redirect to known_addresses
    // TODO: IK not sure if it needed at all - why it needs to be encapsulated in case
    
    MDEBUG("handle_broadcast: posting to known supernodes: ") << "arg { receiver_addresses : '" << boost::algorithm::join(arg.receiver_addresses, "\n") << "'\n arg.callback_uri : '" << arg.callback_uri << "'}";
    MDEBUG("known_addresses ") << boost::algorithm::join(known_addresses, " ");
    
    cryptonote::COMMAND_RPC_REDIRECT_BROADCAST::request redirect_req;
#ifdef UDHT_INFO
    arg.hops = arg.hop;
#endif
    redirect_req.request = arg;
// #if 0  // Do not send redirect broadcast for now        
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
      m_supernode_connections[id].callJsonRpc<cryptonote::COMMAND_RPC_REDIRECT_BROADCAST>("" /*forward to another supernode via local supernode*/, redirect_req, callback_url);
      ++messages_forwarded;
    }
// #endif           
  }
  // XXX DBG
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

SupernodeConnectionManager::Clock::time_point 
SupernodeConnectionManager::get_expiry_time(const SupernodeConnectionManager::SupernodeId &local_sn)
{
  boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
  auto it = m_supernode_connections.find(local_sn);
  assert(it != m_supernode_connections.end());
  return SupernodeConnectionManager::Clock::now() + std::chrono::milliseconds(it->second.redirect_timeout_ms);
}

} // namespace graft
