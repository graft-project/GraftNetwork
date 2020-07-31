#ifndef SUPERNODECONNECTIONMANAGER_H
#define SUPERNODECONNECTIONMANAGER_H

#include "net/http_client.h"
#include "net/jsonrpc_structs.h"
#include "storages/http_abstract_invoke.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "p2p_protocol_defs.h"
#include <cryptonote_core/blockchain.h>
#include <cryptonote_core/cryptonote_core.h>

#include <boost/thread/recursive_mutex.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace cryptonote {
class StakeTransactionProcessor;
}


namespace graft {
class SupernodeConnectionManager 
    :  public cryptonote::BlockAddedHook
     , public cryptonote::AltBlockAddedHook
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
    template<typename command>
    int callJsonRpc(const std::string &method, const typename command::request &req,
                                  typename command::response &resp,
                                  const std::string &endpoint = std::string())
    {
      
      epee::json_rpc::request<typename command::request> json_rpc_req = AUTO_VAL_INIT(json_rpc_req);
      epee::json_rpc::response<typename command::response, std::string> json_rpc_resp = AUTO_VAL_INIT(json_rpc_resp);
      
      json_rpc_req.jsonrpc = "2.0";
      json_rpc_req.id = 0;
      json_rpc_req.method = method;
      json_rpc_req.params = req;
      
      std::string uri = "/" + method;
      // TODO: What is this for?
      if (!endpoint.empty())
      {
        uri = endpoint;
      }
      
      bool r = epee::net_utils::invoke_http_json(this->uri + uri,
                                                 json_rpc_req, json_rpc_resp, this->client,
                                                 std::chrono::milliseconds(size_t(SUPERNODE_HTTP_TIMEOUT_MILLIS)), "POST");
      if (!r)
      {
        return 0;
      }
      resp = json_rpc_resp.result;
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
  
  
  SupernodeConnectionManager(cryptonote::StakeTransactionProcessor &stp);
  virtual ~SupernodeConnectionManager();
  
  void register_supernode(const cryptonote::COMMAND_RPC_REGISTER_SUPERNODE::request& req);
  void add_rta_route(const std::string& dst_id, const std::string& router_id);
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
  bool processBroadcast(typename nodetool::COMMAND_BROADCAST::request &arg, bool &relay_broadcast, uint64_t &messages_sent, uint64_t &messages_forwarded);
  
  template<typename command>
  int invokeAll(const std::string &method, const typename command::request &req,
                
                const std::string &endpoint = std::string())
  {
    boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
    typename command::response resp;
    int ret = 0;
    for (auto& sn : m_supernode_connections)
      ret += sn.second.template callJsonRpc<command>(method, req, resp, endpoint);
    return ret;  
  }
  
  template<typename command>
  int forward(const std::string &method, const typename command::request &req,
                                 const std::string &endpoint = std::string())
  {
    boost::lock_guard<boost::recursive_mutex> guard(m_supernodes_lock);
    typename command::response resp;
    int ret = 0;
    if (req.receiver_addresses.empty())
    {
      for (auto& sn : m_supernode_connections)
        ret += sn.second.template callJsonRpc<command>(method, req, resp, endpoint);
    }
    else
    {
      for (auto& id : req.receiver_addresses) 
      {
        auto it = m_supernode_connections.find(id);
        if (it == m_supernode_connections.end())
          continue;
        SupernodeConnection &conn = it->second;
        ret += conn.template callJsonRpc<command>(method, req, resp, endpoint);
      }
    }
    return ret;
  }  
  bool has_connections() const;
  bool has_routes() const;
  
  std::string dump_routes() const;
  std::string dump_connections() const;
  
  bool block_added(const cryptonote::block &block, const std::vector<cryptonote::transaction> &txs, const struct cryptonote::checkpoint_t *checkpoint = nullptr) override;
  bool alt_block_added(const cryptonote::block &block, const std::vector<cryptonote::transaction> &txs, const struct cryptonote::checkpoint_t *checkpoint = nullptr) override;
  

private:
  Clock::time_point get_expiry_time(const SupernodeId& local_sn);  

private:
  std::map<SupernodeId, SupernodeConnection> m_supernode_connections;
  std::map<SupernodeId, SupernodeRoutes> m_supernode_routes; // recipients ids to redirect to the supernode
  mutable boost::recursive_mutex m_supernodes_lock;
  cryptonote::StakeTransactionProcessor &m_stp;
  // nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> >& m_p2p;
  
  
};

} // namespace graft


#endif // SUPERNODECONNECTIONMANAGER_H
