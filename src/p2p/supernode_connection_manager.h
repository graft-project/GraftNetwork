#ifndef SUPERNODECONNECTIONMANAGER_H
#define SUPERNODECONNECTIONMANAGER_H

#include "net/http_client.h"
#include "net/jsonrpc_structs.h"
#include "storages/http_abstract_invoke.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "p2p_protocol_defs.h"

#include <boost/thread/recursive_mutex.hpp>

#include <chrono>
#include <string>
#include <vector>


namespace graft {

class SupernodeConnectionManager
{
public:
  using Clock = std::chrono::steady_clock;
  using SupernodeId = std::string;
  
  struct SupernodeConnection
  {
    static constexpr size_t SUPERNODE_HTTP_TIMEOUT_MILLIS = 3 * 1000;
    
    std::chrono::steady_clock::time_point expiry_time; // expiry time of this struct
    std::string uri; // base URI (here it is URL without host and port) for forwarding requests to supernode
    std::string redirect_uri; //special uri for UDHT protocol redirection mechanism
    uint32_t redirect_timeout_ms;
    
    epee::net_utils::http::http_simple_client client;
    template<typename request_struct>
    int callJsonRpc(const std::string &method, const typename request_struct::request &body,
                                  const std::string &endpoint = std::string())
    {
      boost::value_initialized<epee::json_rpc::request<typename request_struct::request> > init_req;
      epee::json_rpc::request<typename request_struct::request>& req = static_cast<epee::json_rpc::request<typename request_struct::request> &>(init_req);
      req.jsonrpc = "2.0";
      req.id = 0;
      req.method = method;
      req.params = body;
      
      std::string uri = "/" + method;
      // TODO: What is this for?
      if (!endpoint.empty())
      {
        uri = endpoint;
      }
      typename request_struct::response resp = AUTO_VAL_INIT(resp);
      bool r = epee::net_utils::invoke_http_json(this->uri + uri,
                                                 req, resp, this->client,
                                                 std::chrono::milliseconds(size_t(SUPERNODE_HTTP_TIMEOUT_MILLIS)), "POST");
      if (!r || resp.status == 0)
      {
        return 0;
      }
      return 1;
    }
    
    bool operator==(const SupernodeConnection &other) const;
  };
  
  struct SupernodeRoute
  {
    typename std::map<SupernodeId, SupernodeConnection>::iterator supernode_ptr;
    Clock::time_point expiry_time; // expiry time of this record
  };
  
  using SupernodeRoutes = std::vector<SupernodeRoute>;
  
  
  SupernodeConnectionManager();
  ~SupernodeConnectionManager();
  
  void register_supernode(const cryptonote::COMMAND_RPC_REGISTER_SUPERNODE::request& req);
  void add_rta_route(const std::string& dst_id, const std::string& network_addr);
  void remove_expired_routes();
  std::vector<SupernodeId> connections() const;
  
  /**
   * @brief processBroadcast - delivers RTA messages to local supernodes or forwards to known supernodes
   * @param arg
   * @param should_relay        - returns true if not delivered to all destinations
   * @param messages_sent       - number of messages sent to local supernodes
   * @param messages_forwarded  - number of messages forwarded to remote supernodes
   * @return                    - true on success
   */
  bool processBroadcast(typename nodetool::COMMAND_BROADCAST::request &arg, bool &relay_broadcast, uint64_t &messages_sent);
  
  template<typename request_struct>
  int invokeAll(const std::string &method, const typename request_struct::request &body,
                const std::string &endpoint = std::string())
  {
    boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
    int ret = 0;
    for (auto& sn : m_supernode_connections)
      ret += sn.second.callJsonRpc<request_struct>(method, body, endpoint);
    return ret;  
  }
  
  template<typename request_struct>
  int forward(const std::string &method, const typename request_struct::request &body,
                                 const std::string &endpoint = std::string())
  {
    boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
    int ret = 0;
    if (body.receiver_addresses.empty())
    {
      for (auto& sn : m_supernode_connections)
        ret += sn.second.callJsonRpc<request_struct>(method, body, endpoint);
    }
    else
    {
      for (auto& id : body.receiver_addresses)
      {
        auto it = m_supernode_connections.find(id);
        if (it == m_supernode_connections.end())
          continue;
        SupernodeConnection &conn = it->second;
        ret += conn.callJsonRpc<request_struct>(method, body, endpoint);
      }
    }
    return ret;
  }  
  bool has_connections() const;
  bool has_routes() const;
  
  std::string dump_routes() const;
  std::string dump_connections() const;

private:
  Clock::time_point get_expiry_time(const SupernodeId& local_sn);  

private:
  std::map<SupernodeId, SupernodeConnection> m_supernode_connections;
//  std::map<SupernodeId, SupernodeRoutes> m_supernode_routes; // recipients ids to redirect to the supernode
  mutable boost::recursive_mutex m_supernodes_lock;
  
};

} // namespace graft


#endif // SUPERNODECONNECTIONMANAGER_H
