// Copyright (c) 2014-2019, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma  once 

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include "net/http_server_impl_base.h"
#include "net/http_client.h"
#include "core_rpc_server_commands_defs.h"
#include "cryptonote_core/cryptonote_core.h"
#include "p2p/net_node.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"

#if defined(LOKI_ENABLE_INTEGRATION_TEST_HOOKS)
#include "common/loki_integration_test_hooks.h"
#endif

#undef LOKI_DEFAULT_LOG_CATEGORY
#define LOKI_DEFAULT_LOG_CATEGORY "daemon.rpc"

// yes, epee doesn't properly use its full namespace when calling its
// functions from macros.  *sigh*
using namespace epee;

namespace cryptonote
{
  static constexpr auto rpc_long_poll_timeout = 15s;

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class core_rpc_server: public epee::http_server_impl_base<core_rpc_server>
  {
  public:
    static constexpr int DEFAULT_RPC_THREADS = 2;
    static const command_line::arg_descriptor<bool> arg_public_node;
    static const command_line::arg_descriptor<std::string, false, true, 2> arg_rpc_bind_port;
    static const command_line::arg_descriptor<std::string> arg_rpc_restricted_bind_port;
    static const command_line::arg_descriptor<bool> arg_restricted_rpc;
    static const command_line::arg_descriptor<std::string> arg_rpc_ssl;
    static const command_line::arg_descriptor<std::string> arg_rpc_ssl_private_key;
    static const command_line::arg_descriptor<std::string> arg_rpc_ssl_certificate;
    static const command_line::arg_descriptor<std::string> arg_rpc_ssl_ca_certificates;
    static const command_line::arg_descriptor<std::vector<std::string>> arg_rpc_ssl_allowed_fingerprints;
    static const command_line::arg_descriptor<bool> arg_rpc_ssl_allow_any_cert;
    static const command_line::arg_descriptor<std::string> arg_bootstrap_daemon_address;
    static const command_line::arg_descriptor<std::string> arg_bootstrap_daemon_login;
    static const command_line::arg_descriptor<int> arg_rpc_long_poll_connections;

    typedef epee::net_utils::connection_context_base connection_context;

    core_rpc_server(
        core& cr
      , nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> >& p2p
      );

    static void init_options(boost::program_options::options_description& desc);
    bool init(
        const boost::program_options::variables_map& vm,
        const bool restricted,
        const std::string& port
      );

    network_type nettype() const { return m_core.get_nettype(); }

    CHAIN_HTTP_TO_MAP2(connection_context); //forward http requests to uri map

    BEGIN_URI_MAP2()
      MAP_URI_AUTO_JON2("/get_height", on_get_height, COMMAND_RPC_GET_HEIGHT)
      MAP_URI_AUTO_JON2("/getheight", on_get_height, COMMAND_RPC_GET_HEIGHT)
      MAP_URI_AUTO_BIN2("/get_blocks.bin", on_get_blocks, COMMAND_RPC_GET_BLOCKS_FAST)
      MAP_URI_AUTO_BIN2("/getblocks.bin", on_get_blocks, COMMAND_RPC_GET_BLOCKS_FAST)
      MAP_URI_AUTO_BIN2("/get_blocks_by_height.bin", on_get_blocks_by_height, COMMAND_RPC_GET_BLOCKS_BY_HEIGHT)
      MAP_URI_AUTO_BIN2("/getblocks_by_height.bin", on_get_blocks_by_height, COMMAND_RPC_GET_BLOCKS_BY_HEIGHT)
      MAP_URI_AUTO_BIN2("/get_hashes.bin", on_get_hashes, COMMAND_RPC_GET_HASHES_FAST)
      MAP_URI_AUTO_BIN2("/gethashes.bin", on_get_hashes, COMMAND_RPC_GET_HASHES_FAST)
      MAP_URI_AUTO_BIN2("/get_o_indexes.bin", on_get_indexes, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES)      
      MAP_URI_AUTO_BIN2("/get_outs.bin", on_get_outs_bin, COMMAND_RPC_GET_OUTPUTS_BIN)
      MAP_URI_AUTO_JON2("/get_transactions", on_get_transactions, COMMAND_RPC_GET_TRANSACTIONS)
      MAP_URI_AUTO_JON2("/gettransactions", on_get_transactions, COMMAND_RPC_GET_TRANSACTIONS)
      MAP_URI_AUTO_JON2("/get_alt_blocks_hashes", on_get_alt_blocks_hashes, COMMAND_RPC_GET_ALT_BLOCKS_HASHES)
      MAP_URI_AUTO_JON2("/is_key_image_spent", on_is_key_image_spent, COMMAND_RPC_IS_KEY_IMAGE_SPENT)
      MAP_URI_AUTO_JON2("/send_raw_transaction", on_send_raw_tx, COMMAND_RPC_SEND_RAW_TX)
      MAP_URI_AUTO_JON2("/sendrawtransaction", on_send_raw_tx, COMMAND_RPC_SEND_RAW_TX)
      MAP_URI_AUTO_JON2_IF("/start_mining", on_start_mining, COMMAND_RPC_START_MINING, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/stop_mining", on_stop_mining, COMMAND_RPC_STOP_MINING, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/mining_status", on_mining_status, COMMAND_RPC_MINING_STATUS, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/save_bc", on_save_bc, COMMAND_RPC_SAVE_BC, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/get_peer_list", on_get_peer_list, COMMAND_RPC_GET_PEER_LIST, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/set_log_hash_rate", on_set_log_hash_rate, COMMAND_RPC_SET_LOG_HASH_RATE, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/set_log_level", on_set_log_level, COMMAND_RPC_SET_LOG_LEVEL, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/set_log_categories", on_set_log_categories, COMMAND_RPC_SET_LOG_CATEGORIES, !m_restricted)
      MAP_URI_AUTO_JON2("/get_transaction_pool", on_get_transaction_pool, COMMAND_RPC_GET_TRANSACTION_POOL)
      MAP_URI_AUTO_JON2("/get_transaction_pool_hashes.bin", on_get_transaction_pool_hashes_bin, COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN)
      MAP_URI_AUTO_JON2("/get_transaction_pool_hashes", on_get_transaction_pool_hashes, COMMAND_RPC_GET_TRANSACTION_POOL_HASHES)
      MAP_URI_AUTO_JON2("/get_transaction_pool_stats", on_get_transaction_pool_stats, COMMAND_RPC_GET_TRANSACTION_POOL_STATS)
      MAP_URI_AUTO_JON2_IF("/stop_daemon", on_stop_daemon, COMMAND_RPC_STOP_DAEMON, !m_restricted)
      MAP_URI_AUTO_JON2("/get_info", on_get_info, COMMAND_RPC_GET_INFO)
      MAP_URI_AUTO_JON2("/getinfo", on_get_info, COMMAND_RPC_GET_INFO)
      MAP_URI_AUTO_JON2_IF("/get_net_stats", on_get_net_stats, COMMAND_RPC_GET_NET_STATS, !m_restricted)
      MAP_URI_AUTO_JON2("/get_limit", on_get_limit, COMMAND_RPC_GET_LIMIT)
      MAP_URI_AUTO_JON2_IF("/set_limit", on_set_limit, COMMAND_RPC_SET_LIMIT, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/out_peers", on_out_peers, COMMAND_RPC_OUT_PEERS, !m_restricted)
      MAP_URI_AUTO_JON2_IF("/in_peers", on_in_peers, COMMAND_RPC_IN_PEERS, !m_restricted)
      MAP_URI_AUTO_JON2("/get_outs", on_get_outs, COMMAND_RPC_GET_OUTPUTS)      
      MAP_URI_AUTO_JON2_IF("/update", on_update, COMMAND_RPC_UPDATE, !m_restricted)
      MAP_URI_AUTO_BIN2("/get_output_distribution.bin", on_get_output_distribution_bin, COMMAND_RPC_GET_OUTPUT_DISTRIBUTION)
      MAP_URI_AUTO_BIN2("/get_output_blacklist.bin", on_get_output_blacklist_bin, COMMAND_RPC_GET_OUTPUT_BLACKLIST)
      MAP_URI_AUTO_JON2_IF("/pop_blocks", on_pop_blocks, COMMAND_RPC_POP_BLOCKS, !m_restricted)
      BEGIN_JSON_RPC_MAP("/json_rpc")
        MAP_JON_RPC("get_block_count",           on_getblockcount,              COMMAND_RPC_GETBLOCKCOUNT)
        MAP_JON_RPC("getblockcount",             on_getblockcount,              COMMAND_RPC_GETBLOCKCOUNT)
        MAP_JON_RPC_WE("on_get_block_hash",      on_getblockhash,               COMMAND_RPC_GETBLOCKHASH)
        MAP_JON_RPC_WE("on_getblockhash",        on_getblockhash,               COMMAND_RPC_GETBLOCKHASH)
        MAP_JON_RPC_WE("get_block_template",     on_getblocktemplate,           COMMAND_RPC_GETBLOCKTEMPLATE)
        MAP_JON_RPC_WE("getblocktemplate",       on_getblocktemplate,           COMMAND_RPC_GETBLOCKTEMPLATE)
        MAP_JON_RPC_WE("submit_block",           on_submitblock,                COMMAND_RPC_SUBMITBLOCK)
        MAP_JON_RPC_WE("submitblock",            on_submitblock,                COMMAND_RPC_SUBMITBLOCK)
        MAP_JON_RPC_WE_IF("generateblocks",         on_generateblocks,             COMMAND_RPC_GENERATEBLOCKS, !m_restricted)
        MAP_JON_RPC_WE("get_last_block_header",  on_get_last_block_header,      COMMAND_RPC_GET_LAST_BLOCK_HEADER)
        MAP_JON_RPC_WE("getlastblockheader",     on_get_last_block_header,      COMMAND_RPC_GET_LAST_BLOCK_HEADER)
        MAP_JON_RPC_WE("get_block_header_by_hash", on_get_block_header_by_hash,   COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH)
        MAP_JON_RPC_WE("getblockheaderbyhash",   on_get_block_header_by_hash,   COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH)
        MAP_JON_RPC_WE("get_block_header_by_height", on_get_block_header_by_height, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT)
        MAP_JON_RPC_WE("getblockheaderbyheight", on_get_block_header_by_height, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT)
        MAP_JON_RPC_WE("get_block_headers_range", on_get_block_headers_range,    COMMAND_RPC_GET_BLOCK_HEADERS_RANGE)
        MAP_JON_RPC_WE("getblockheadersrange",   on_get_block_headers_range,    COMMAND_RPC_GET_BLOCK_HEADERS_RANGE)
        MAP_JON_RPC_WE("get_block",              on_get_block,                 COMMAND_RPC_GET_BLOCK)
        MAP_JON_RPC_WE("getblock",                on_get_block,                 COMMAND_RPC_GET_BLOCK)
        MAP_JON_RPC_WE_IF("get_connections",     on_get_connections,            COMMAND_RPC_GET_CONNECTIONS, !m_restricted)
        MAP_JON_RPC_WE("get_info",               on_get_info_json,              COMMAND_RPC_GET_INFO)
        MAP_JON_RPC_WE("hard_fork_info",         on_hard_fork_info,             COMMAND_RPC_HARD_FORK_INFO)
        MAP_JON_RPC_WE_IF("set_bans",            on_set_bans,                   COMMAND_RPC_SETBANS, !m_restricted)
        MAP_JON_RPC_WE_IF("get_bans",            on_get_bans,                   COMMAND_RPC_GETBANS, !m_restricted)
        MAP_JON_RPC_WE_IF("banned",              on_banned,                     COMMAND_RPC_BANNED, !m_restricted)
        MAP_JON_RPC_WE_IF("flush_txpool",        on_flush_txpool,               COMMAND_RPC_FLUSH_TRANSACTION_POOL, !m_restricted)
        MAP_JON_RPC_WE("get_output_histogram",   on_get_output_histogram,       COMMAND_RPC_GET_OUTPUT_HISTOGRAM)
        MAP_JON_RPC_WE("get_version",            on_get_version,                COMMAND_RPC_GET_VERSION)
        MAP_JON_RPC_WE_IF("get_coinbase_tx_sum", on_get_coinbase_tx_sum,        COMMAND_RPC_GET_COINBASE_TX_SUM, !m_restricted)
        MAP_JON_RPC_WE("get_fee_estimate",       on_get_base_fee_estimate,      COMMAND_RPC_GET_BASE_FEE_ESTIMATE)
        MAP_JON_RPC_WE_IF("get_alternate_chains",on_get_alternate_chains,       COMMAND_RPC_GET_ALTERNATE_CHAINS, !m_restricted)
        MAP_JON_RPC_WE_IF("relay_tx",            on_relay_tx,                   COMMAND_RPC_RELAY_TX, !m_restricted)
        MAP_JON_RPC_WE_IF("sync_info",           on_sync_info,                  COMMAND_RPC_SYNC_INFO, !m_restricted)
        MAP_JON_RPC_WE("get_txpool_backlog",     on_get_txpool_backlog,         COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG)
        MAP_JON_RPC_WE("get_output_distribution", on_get_output_distribution, COMMAND_RPC_GET_OUTPUT_DISTRIBUTION)
        MAP_JON_RPC_WE_IF("prune_blockchain",    on_prune_blockchain,           COMMAND_RPC_PRUNE_BLOCKCHAIN, !m_restricted)

        //
        // Loki
        //
        MAP_JON_RPC_WE("get_quorum_state",                          on_get_quorum_state, COMMAND_RPC_GET_QUORUM_STATE)
        MAP_JON_RPC_WE_IF("get_service_node_registration_cmd_raw",  on_get_service_node_registration_cmd_raw, COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW, !m_restricted)
        MAP_JON_RPC_WE("get_service_node_blacklisted_key_images",   on_get_service_node_blacklisted_key_images, COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES)
        MAP_JON_RPC_WE_IF("get_service_node_registration_cmd",      on_get_service_node_registration_cmd, COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD, !m_restricted)
        MAP_JON_RPC_WE_IF("get_service_node_key",                   on_get_service_node_key, COMMAND_RPC_GET_SERVICE_NODE_KEY, !m_restricted)
        MAP_JON_RPC_WE_IF("get_service_node_privkey",               on_get_service_node_privkey, COMMAND_RPC_GET_SERVICE_NODE_PRIVKEY, !m_restricted)
        MAP_JON_RPC_WE("get_service_node_status",                         on_get_service_node_status, COMMAND_RPC_GET_SERVICE_NODE_STATUS)
        MAP_JON_RPC_WE("get_service_nodes",                         on_get_service_nodes, COMMAND_RPC_GET_SERVICE_NODES)
        MAP_JON_RPC_WE("get_all_service_nodes",                     on_get_all_service_nodes, COMMAND_RPC_GET_SERVICE_NODES)
        MAP_JON_RPC_WE("get_n_service_nodes",                       on_get_n_service_nodes, COMMAND_RPC_GET_N_SERVICE_NODES)
        MAP_JON_RPC_WE("get_staking_requirement",                   on_get_staking_requirement, COMMAND_RPC_GET_STAKING_REQUIREMENT)
        MAP_JON_RPC_WE("get_checkpoints",                           on_get_checkpoints, COMMAND_RPC_GET_CHECKPOINTS)
        MAP_JON_RPC_WE_IF("perform_blockchain_test",                on_perform_blockchain_test, COMMAND_RPC_PERFORM_BLOCKCHAIN_TEST, !m_restricted)
        MAP_JON_RPC_WE_IF("storage_server_ping",                    on_storage_server_ping, COMMAND_RPC_STORAGE_SERVER_PING, !m_restricted)
        MAP_JON_RPC_WE_IF("lokinet_ping",                           on_lokinet_ping, COMMAND_RPC_LOKINET_PING, !m_restricted)
        MAP_JON_RPC_WE("get_service_nodes_state_changes",           on_get_service_nodes_state_changes, COMMAND_RPC_GET_SN_STATE_CHANGES)
        MAP_JON_RPC_WE_IF("report_peer_storage_server_status",      on_report_peer_storage_server_status, COMMAND_RPC_REPORT_PEER_SS_STATUS, !m_restricted)
        MAP_JON_RPC_WE_IF("test_trigger_p2p_resync",                on_test_trigger_p2p_resync, COMMAND_RPC_TEST_TRIGGER_P2P_RESYNC, !m_restricted)
        MAP_JON_RPC_WE("lns_names_to_owners",                       on_lns_names_to_owners, COMMAND_RPC_LNS_NAMES_TO_OWNERS)
        MAP_JON_RPC_WE("lns_owners_to_names",                       on_lns_owners_to_names, COMMAND_RPC_LNS_OWNERS_TO_NAMES)
      END_JSON_RPC_MAP()
      // Graft RTA handlers start here
      BEGIN_JSON_RPC_MAP("/json_rpc/rta")
        MAP_JON_RPC_WE_IF("send_supernode_announce",on_supernode_announce,         COMMAND_RPC_SUPERNODE_ANNOUNCE, !m_restricted)
        MAP_JON_RPC_WE_IF("broadcast",              on_broadcast,                  COMMAND_RPC_BROADCAST, !m_restricted)
        MAP_JON_RPC_WE_IF("multicast",              on_multicast,                  COMMAND_RPC_MULTICAST, !m_restricted)
        MAP_JON_RPC_WE_IF("unicast",                on_unicast,                    COMMAND_RPC_UNICAST, !m_restricted)

        MAP_JON_RPC_WE_IF("get_tunnels",              on_get_tunnels,              COMMAND_RPC_TUNNEL_DATA, !m_restricted)
        MAP_JON_RPC_WE_IF("send_supernode_stakes",    on_supernode_stakes,         COMMAND_RPC_SUPERNODE_GET_STAKES, !m_restricted)
        MAP_JON_RPC_WE_IF("send_supernode_blockchain_based_list", on_supernode_blockchain_based_list,  COMMAND_RPC_SUPERNODE_GET_BLOCKCHAIN_BASED_LIST, !m_restricted)
        MAP_JON_RPC_WE_IF("get_stats", on_get_rta_stats,  COMMAND_RPC_RTA_STATS, !m_restricted)
      END_JSON_RPC_MAP()
    END_URI_MAP2()

    bool on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res, const connection_context *ctx = NULL);
    bool on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res, const connection_context *ctx = NULL);
    bool on_get_alt_blocks_hashes(const COMMAND_RPC_GET_ALT_BLOCKS_HASHES::request& req, COMMAND_RPC_GET_ALT_BLOCKS_HASHES::response& res, const connection_context *ctx = NULL);
    bool on_get_blocks_by_height(const COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::response& res, const connection_context *ctx = NULL);
    bool on_get_hashes(const COMMAND_RPC_GET_HASHES_FAST::request& req, COMMAND_RPC_GET_HASHES_FAST::response& res, const connection_context *ctx = NULL);
    bool on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res, const connection_context *ctx = NULL);
    bool on_is_key_image_spent(const COMMAND_RPC_IS_KEY_IMAGE_SPENT::request& req, COMMAND_RPC_IS_KEY_IMAGE_SPENT::response& res, const connection_context *ctx = NULL);
    bool on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res, const connection_context *ctx = NULL);
    bool on_send_raw_tx(const COMMAND_RPC_SEND_RAW_TX::request& req, COMMAND_RPC_SEND_RAW_TX::response& res, const connection_context *ctx = NULL);
    bool on_start_mining(const COMMAND_RPC_START_MINING::request& req, COMMAND_RPC_START_MINING::response& res, const connection_context *ctx = NULL);
    bool on_stop_mining(const COMMAND_RPC_STOP_MINING::request& req, COMMAND_RPC_STOP_MINING::response& res, const connection_context *ctx = NULL);
    bool on_mining_status(const COMMAND_RPC_MINING_STATUS::request& req, COMMAND_RPC_MINING_STATUS::response& res, const connection_context *ctx = NULL);
    bool on_get_outs_bin(const COMMAND_RPC_GET_OUTPUTS_BIN::request& req, COMMAND_RPC_GET_OUTPUTS_BIN::response& res, const connection_context *ctx = NULL);
    bool on_get_outs(const COMMAND_RPC_GET_OUTPUTS::request& req, COMMAND_RPC_GET_OUTPUTS::response& res, const connection_context *ctx = NULL);
    bool on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res, const connection_context *ctx = NULL);
    bool on_get_net_stats(const COMMAND_RPC_GET_NET_STATS::request& req, COMMAND_RPC_GET_NET_STATS::response& res, const connection_context *ctx = NULL);
    bool on_save_bc(const COMMAND_RPC_SAVE_BC::request& req, COMMAND_RPC_SAVE_BC::response& res, const connection_context *ctx = NULL);
    bool on_get_peer_list(const COMMAND_RPC_GET_PEER_LIST::request& req, COMMAND_RPC_GET_PEER_LIST::response& res, const connection_context *ctx = NULL);
    bool on_set_log_hash_rate(const COMMAND_RPC_SET_LOG_HASH_RATE::request& req, COMMAND_RPC_SET_LOG_HASH_RATE::response& res, const connection_context *ctx = NULL);
    bool on_set_log_level(const COMMAND_RPC_SET_LOG_LEVEL::request& req, COMMAND_RPC_SET_LOG_LEVEL::response& res, const connection_context *ctx = NULL);
    bool on_set_log_categories(const COMMAND_RPC_SET_LOG_CATEGORIES::request& req, COMMAND_RPC_SET_LOG_CATEGORIES::response& res, const connection_context *ctx = NULL);
    bool on_get_transaction_pool(const COMMAND_RPC_GET_TRANSACTION_POOL::request& req, COMMAND_RPC_GET_TRANSACTION_POOL::response& res, const connection_context *ctx = NULL);
    bool on_get_transaction_pool_hashes_bin(const COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::request& req, COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::response& res, const connection_context *ctx = NULL);
    bool on_get_transaction_pool_hashes(const COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::request& req, COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::response& res, const connection_context *ctx = NULL);
    bool on_get_transaction_pool_stats(const COMMAND_RPC_GET_TRANSACTION_POOL_STATS::request& req, COMMAND_RPC_GET_TRANSACTION_POOL_STATS::response& res, const connection_context *ctx = NULL);
    bool on_stop_daemon(const COMMAND_RPC_STOP_DAEMON::request& req, COMMAND_RPC_STOP_DAEMON::response& res, const connection_context *ctx = NULL);
    bool on_get_limit(const COMMAND_RPC_GET_LIMIT::request& req, COMMAND_RPC_GET_LIMIT::response& res, const connection_context *ctx = NULL);
    bool on_set_limit(const COMMAND_RPC_SET_LIMIT::request& req, COMMAND_RPC_SET_LIMIT::response& res, const connection_context *ctx = NULL);
    bool on_out_peers(const COMMAND_RPC_OUT_PEERS::request& req, COMMAND_RPC_OUT_PEERS::response& res, const connection_context *ctx = NULL);
    bool on_in_peers(const COMMAND_RPC_IN_PEERS::request& req, COMMAND_RPC_IN_PEERS::response& res, const connection_context *ctx = NULL);
    bool on_update(const COMMAND_RPC_UPDATE::request& req, COMMAND_RPC_UPDATE::response& res, const connection_context *ctx = NULL);
    bool on_get_output_distribution_bin(const COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request& req, COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response& res, const connection_context *ctx = NULL);
    bool on_pop_blocks(const COMMAND_RPC_POP_BLOCKS::request& req, COMMAND_RPC_POP_BLOCKS::response& res, const connection_context *ctx = NULL);

    //
    // Loki
    //
    bool on_get_output_blacklist_bin(const COMMAND_RPC_GET_OUTPUT_BLACKLIST::request& req, COMMAND_RPC_GET_OUTPUT_BLACKLIST::response& res, const connection_context *ctx = NULL);

    //json_rpc
    bool on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res, const connection_context *ctx = NULL);
    bool on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request& req, COMMAND_RPC_GETBLOCKHASH::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_generateblocks(const COMMAND_RPC_GENERATEBLOCKS::request& req, COMMAND_RPC_GENERATEBLOCKS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_block_headers_range(const COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::request& req, COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_block(const COMMAND_RPC_GET_BLOCK::request& req, COMMAND_RPC_GET_BLOCK::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_connections(const COMMAND_RPC_GET_CONNECTIONS::request& req, COMMAND_RPC_GET_CONNECTIONS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_info_json(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_hard_fork_info(const COMMAND_RPC_HARD_FORK_INFO::request& req, COMMAND_RPC_HARD_FORK_INFO::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_set_bans(const COMMAND_RPC_SETBANS::request& req, COMMAND_RPC_SETBANS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_bans(const COMMAND_RPC_GETBANS::request& req, COMMAND_RPC_GETBANS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_banned(const COMMAND_RPC_BANNED::request& req, COMMAND_RPC_BANNED::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_flush_txpool(const COMMAND_RPC_FLUSH_TRANSACTION_POOL::request& req, COMMAND_RPC_FLUSH_TRANSACTION_POOL::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_output_histogram(const COMMAND_RPC_GET_OUTPUT_HISTOGRAM::request& req, COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_version(const COMMAND_RPC_GET_VERSION::request& req, COMMAND_RPC_GET_VERSION::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_coinbase_tx_sum(const COMMAND_RPC_GET_COINBASE_TX_SUM::request& req, COMMAND_RPC_GET_COINBASE_TX_SUM::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_base_fee_estimate(const COMMAND_RPC_GET_BASE_FEE_ESTIMATE::request& req, COMMAND_RPC_GET_BASE_FEE_ESTIMATE::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_alternate_chains(const COMMAND_RPC_GET_ALTERNATE_CHAINS::request& req, COMMAND_RPC_GET_ALTERNATE_CHAINS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_relay_tx(const COMMAND_RPC_RELAY_TX::request& req, COMMAND_RPC_RELAY_TX::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_sync_info(const COMMAND_RPC_SYNC_INFO::request& req, COMMAND_RPC_SYNC_INFO::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_txpool_backlog(const COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG::request& req, COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_output_distribution(const COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request& req, COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_prune_blockchain(const COMMAND_RPC_PRUNE_BLOCKCHAIN::request& req, COMMAND_RPC_PRUNE_BLOCKCHAIN::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);

    //
    // Loki
    //
    bool on_get_quorum_state(const COMMAND_RPC_GET_QUORUM_STATE::request& req, COMMAND_RPC_GET_QUORUM_STATE::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_service_node_registration_cmd_raw(const COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW::request& req, COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_service_node_registration_cmd(const COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD::request& req, COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_service_node_blacklisted_key_images(const COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES::request& req, COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES::response& res, epee::json_rpc::error &error_resp, const connection_context *ctx = NULL);
    bool on_get_service_node_key(const COMMAND_RPC_GET_SERVICE_NODE_KEY::request& req, COMMAND_RPC_GET_SERVICE_NODE_KEY::response& res, epee::json_rpc::error &error_resp, const connection_context *ctx = NULL);
    bool on_get_service_node_privkey(const COMMAND_RPC_GET_SERVICE_NODE_PRIVKEY::request& req, COMMAND_RPC_GET_SERVICE_NODE_PRIVKEY::response& res, epee::json_rpc::error &error_resp, const connection_context *ctx = NULL);
    bool on_get_service_node_status(const COMMAND_RPC_GET_SERVICE_NODE_STATUS::request& req, COMMAND_RPC_GET_SERVICE_NODE_STATUS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_service_nodes(const COMMAND_RPC_GET_SERVICE_NODES::request& req, COMMAND_RPC_GET_SERVICE_NODES::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_n_service_nodes(const COMMAND_RPC_GET_N_SERVICE_NODES::request& req, COMMAND_RPC_GET_N_SERVICE_NODES::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_all_service_nodes(const COMMAND_RPC_GET_SERVICE_NODES::request& req, COMMAND_RPC_GET_SERVICE_NODES::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_staking_requirement(const COMMAND_RPC_GET_STAKING_REQUIREMENT::request& req, COMMAND_RPC_GET_STAKING_REQUIREMENT::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    /// Provide a proof that this node holds the blockchain
    bool on_perform_blockchain_test(const COMMAND_RPC_PERFORM_BLOCKCHAIN_TEST::request& req, COMMAND_RPC_PERFORM_BLOCKCHAIN_TEST::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_storage_server_ping(const COMMAND_RPC_STORAGE_SERVER_PING::request& req, COMMAND_RPC_STORAGE_SERVER_PING::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_lokinet_ping(const COMMAND_RPC_LOKINET_PING::request& req, COMMAND_RPC_LOKINET_PING::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_checkpoints(const COMMAND_RPC_GET_CHECKPOINTS::request& req, COMMAND_RPC_GET_CHECKPOINTS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_get_service_nodes_state_changes(const COMMAND_RPC_GET_SN_STATE_CHANGES::request& req, COMMAND_RPC_GET_SN_STATE_CHANGES::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_report_peer_storage_server_status(const COMMAND_RPC_REPORT_PEER_SS_STATUS::request& req, COMMAND_RPC_REPORT_PEER_SS_STATUS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_test_trigger_p2p_resync(const COMMAND_RPC_TEST_TRIGGER_P2P_RESYNC::request& req, COMMAND_RPC_TEST_TRIGGER_P2P_RESYNC::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx = NULL);
    bool on_lns_names_to_owners(const COMMAND_RPC_LNS_NAMES_TO_OWNERS::request &req, COMMAND_RPC_LNS_NAMES_TO_OWNERS::response &res, epee::json_rpc::error &error_resp, const connection_context *ctx = NULL);
    bool on_lns_owners_to_names(const COMMAND_RPC_LNS_OWNERS_TO_NAMES::request &req, COMMAND_RPC_LNS_OWNERS_TO_NAMES::response &res, epee::json_rpc::error &error_resp, const connection_context *ctx = NULL);
    //-----------------------
    // RTA
    bool on_supernode_announce(const COMMAND_RPC_SUPERNODE_ANNOUNCE::request& req, COMMAND_RPC_SUPERNODE_ANNOUNCE::response& res, epee::json_rpc::error& error_resp);
    bool on_supernode_stakes(const COMMAND_RPC_SUPERNODE_GET_STAKES::request& req, COMMAND_RPC_SUPERNODE_GET_STAKES::response& res, epee::json_rpc::error& error_resp);
    bool on_supernode_blockchain_based_list(const COMMAND_RPC_SUPERNODE_GET_BLOCKCHAIN_BASED_LIST::request& req, COMMAND_RPC_SUPERNODE_GET_BLOCKCHAIN_BASED_LIST::response& res, epee::json_rpc::error& error_resp);
    bool on_broadcast(const COMMAND_RPC_BROADCAST::request &req, COMMAND_RPC_BROADCAST::response &res, epee::json_rpc::error &error_resp);
    bool on_multicast(const COMMAND_RPC_MULTICAST::request &req, COMMAND_RPC_MULTICAST::response &res, epee::json_rpc::error &error_resp);
    bool on_unicast(const COMMAND_RPC_UNICAST::request &req, COMMAND_RPC_UNICAST::response &res, epee::json_rpc::error &error_resp);

    bool on_get_tunnels(const COMMAND_RPC_TUNNEL_DATA::request &req, COMMAND_RPC_TUNNEL_DATA::response &res, epee::json_rpc::error &error_resp);
    bool on_get_rta_stats(const COMMAND_RPC_RTA_STATS::request &req, COMMAND_RPC_RTA_STATS::response &res, epee::json_rpc::error &error_resp);

#if defined(LOKI_ENABLE_INTEGRATION_TEST_HOOKS)
    void on_relay_uptime_and_votes()
    {
      m_core.submit_uptime_proof();
      m_core.relay_service_node_votes();
      std::cout << "Votes and uptime relayed";
      integration_test::write_buffered_stdout();
    }

    void on_debug_mine_n_blocks(std::string const &address, uint64_t num_blocks)
    {
      cryptonote::miner &miner = m_core.get_miner();
      if (miner.is_mining())
      {
        std::cout << "Already mining";
        return;
      }

      cryptonote::address_parse_info info;
      if(!get_account_address_from_str(info, m_core.get_nettype(), address))
      {
        std::cout << "Failed, wrong address";
        return;
      }

      for (uint64_t i = 0; i < num_blocks; i++)
      {
        if(!miner.debug_mine_singular_block(info.address))
        {
          std::cout << "Failed, mining not started";
          return;
        }
      }

      std::cout << "Mining stopped in daemon";
    }
#endif

    int              m_max_long_poll_connections;
private:
    std::atomic<int> m_long_poll_active_connections;

    bool check_core_busy();
    bool check_core_ready();

    template<typename response>
    void fill_sn_response_entry(response &entry, const service_nodes::service_node_pubkey_info &sn_info, uint64_t current_height);
    
    //utils
    uint64_t get_block_reward(const block& blk);
    bool fill_block_header_response(const block& blk, bool orphan_status, uint64_t height, const crypto::hash& hash, block_header_response& response, bool fill_pow_hash);
    enum invoke_http_mode { JON, BIN, JON_RPC };
    template <typename COMMAND_TYPE>
    bool use_bootstrap_daemon_if_necessary(const invoke_http_mode &mode, const std::string &command_name, const typename COMMAND_TYPE::request& req, typename COMMAND_TYPE::response& res, bool &r);
    
    core& m_core;
    nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> >& m_p2p;
    std::string m_bootstrap_daemon_address;
    epee::net_utils::http::http_simple_client m_http_client;
    boost::shared_mutex m_bootstrap_daemon_mutex;
    bool m_should_use_bootstrap_daemon;
    std::chrono::system_clock::time_point m_bootstrap_height_check_time;
    bool m_was_bootstrap_ever_used;
    network_type m_nettype;
    bool m_restricted;
  };
}

BOOST_CLASS_VERSION(nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> >, 1);
