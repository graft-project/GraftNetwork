// Copyright (c) 2018, The Graft Project
// Copyright (c) 2014-2019, The Monero Project
// Copyright (c)      2018, The Loki Project
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

#include <boost/preprocessor/stringize.hpp>
#include <boost/endian/conversion.hpp>
#include <algorithm>
#include <cstring>
#include "include_base_utils.h"
#include "string_tools.h"
using namespace epee;

#include "core_rpc_server.h"
#include "common/command_line.h"
#include "common/updates.h"
#include "common/download.h"
#include "common/loki.h"
#include "common/util.h"
#include "common/perf_timer.h"
#include "common/random.h"
#include "common/base32z.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_core/tx_sanity_check.h"
#include "misc_language.h"
#include "net/parse.h"
#include "storages/http_abstract_invoke.h"
#include "crypto/hash.h"
#include "rpc/rpc_args.h"
#include "rpc/rpc_handler.h"
#include "core_rpc_server_error_codes.h"
#include "p2p/net_node.h"
#include "version.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "daemon.rpc"

#define MAX_RESTRICTED_FAKE_OUTS_COUNT 40
#define MAX_RESTRICTED_GLOBAL_FAKE_OUTS_COUNT 5000

#define OUTPUT_HISTOGRAM_RECENT_CUTOFF_RESTRICTION (3 * 86400) // 3 days max, the wallet requests 1.8 days

namespace
{
  uint64_t round_up(uint64_t value, uint64_t quantum)
  {
    return (value + quantum - 1) / quantum * quantum;
  }
}

namespace std
{
template <>
struct hash<lns::generic_owner>
{
  static_assert(sizeof(lns::generic_owner) >= sizeof(std::size_t) && alignof(lns::generic_owner) >= alignof(std::size_t),
                "Size and alignment of hash must be at least that of size_t");
  std::size_t operator()(const lns::generic_owner &v) const { return reinterpret_cast<const std::size_t &>(v); }
};
} // namespace std

namespace cryptonote
{
  // TODO: move to some utils/helpers library
  bool validate_wallet(const std::string &wallet_addr, bool testnet)
  {
    cryptonote::address_parse_info acc = AUTO_VAL_INIT(acc);
    return wallet_addr.size() && cryptonote::get_account_address_from_str(acc, testnet ? cryptonote::TESTNET : cryptonote::MAINNET, wallet_addr);
  }
  //-----------------------------------------------------------------------------------
  void core_rpc_server::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_rpc_bind_port);
    command_line::add_arg(desc, arg_rpc_restricted_bind_port);
    command_line::add_arg(desc, arg_restricted_rpc);
    command_line::add_arg(desc, arg_bootstrap_daemon_address);
    command_line::add_arg(desc, arg_bootstrap_daemon_login);
    command_line::add_arg(desc, arg_rpc_long_poll_connections);
    cryptonote::rpc_args::init_options(desc, true);
  }
  //------------------------------------------------------------------------------------------------------------------------------
  core_rpc_server::core_rpc_server(
      core& cr
    , nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> >& p2p
    )
    : m_core(cr)
    , m_p2p(p2p)
  {}
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::init(
      const boost::program_options::variables_map& vm
      , const bool restricted
      , const std::string& port
    )
  {
    m_restricted = restricted;
    m_net_server.set_threads_prefix("RPC");
    m_max_long_poll_connections = command_line::get_arg(vm, arg_rpc_long_poll_connections);

    auto rpc_config = cryptonote::rpc_args::process(vm, true);
    if (!rpc_config)
      return false;

    m_bootstrap_daemon_address = command_line::get_arg(vm, arg_bootstrap_daemon_address);
    if (!m_bootstrap_daemon_address.empty())
    {
      const std::string &bootstrap_daemon_login = command_line::get_arg(vm, arg_bootstrap_daemon_login);
      const auto loc = bootstrap_daemon_login.find(':');
      if (!bootstrap_daemon_login.empty() && loc != std::string::npos)
      {
        epee::net_utils::http::login login;
        login.username = bootstrap_daemon_login.substr(0, loc);
        login.password = bootstrap_daemon_login.substr(loc + 1);
        m_http_client.set_server(m_bootstrap_daemon_address, login, epee::net_utils::ssl_support_t::e_ssl_support_autodetect);
      }
      else
      {
        m_http_client.set_server(m_bootstrap_daemon_address, boost::none, epee::net_utils::ssl_support_t::e_ssl_support_autodetect);
      }
      m_should_use_bootstrap_daemon = true;
    }
    else
    {
      m_should_use_bootstrap_daemon = false;
    }
    m_was_bootstrap_ever_used = false;

    boost::optional<epee::net_utils::http::login> http_login{};

    if (rpc_config->login)
      http_login.emplace(std::move(rpc_config->login->username), std::move(rpc_config->login->password).password());

    auto rng = [](size_t len, uint8_t *ptr){ return crypto::rand(len, ptr); };
    return epee::http_server_impl_base<core_rpc_server, connection_context>::init(
      rng, std::move(port), std::move(rpc_config->bind_ip),
      std::move(rpc_config->bind_ipv6_address), std::move(rpc_config->use_ipv6), std::move(rpc_config->require_ipv4),
      std::move(rpc_config->access_control_origins), std::move(http_login), std::move(rpc_config->ssl_options)
    );
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::check_core_ready()
  {
    if(!m_p2p.get_payload_object().is_synchronized())
    {
      return false;
    }
    return true;
  }
#define CHECK_CORE_READY() do { if(!check_core_ready()){res.status =  CORE_RPC_STATUS_BUSY;return true;} } while(0)

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_height);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_HEIGHT>(invoke_http_mode::JON, "/getheight", req, res, r))
      return r;

    crypto::hash hash;
    m_core.get_blockchain_top(res.height, hash);
    ++res.height; // block height to chain height
    res.hash = string_tools::pod_to_hex(hash);
    res.status = CORE_RPC_STATUS_OK;

    res.immutable_height = 0;
    cryptonote::checkpoint_t checkpoint;
    if (m_core.get_blockchain_storage().get_db().get_immutable_checkpoint(&checkpoint, res.height - 1))
    {
      res.immutable_height = checkpoint.height;
      res.immutable_hash   = string_tools::pod_to_hex(checkpoint.block_hash);
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_info);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_INFO>(invoke_http_mode::JON, "/getinfo", req, res, r))
    {
      res.bootstrap_daemon_address = m_bootstrap_daemon_address;
      crypto::hash top_hash;
      m_core.get_blockchain_top(res.height_without_bootstrap, top_hash);
      ++res.height_without_bootstrap; // turn top block height into blockchain height
      res.was_bootstrap_ever_used = true;
      return r;
    }

    const bool restricted = m_restricted && ctx;

    crypto::hash top_hash;
    m_core.get_blockchain_top(res.height, top_hash);
    ++res.height; // turn top block height into blockchain height
    res.top_block_hash = string_tools::pod_to_hex(top_hash);
    res.target_height = m_core.get_target_blockchain_height();

    res.immutable_height = 0;
    cryptonote::checkpoint_t checkpoint;
    if (m_core.get_blockchain_storage().get_db().get_immutable_checkpoint(&checkpoint, res.height - 1))
    {
      res.immutable_height     = checkpoint.height;
      res.immutable_block_hash = string_tools::pod_to_hex(checkpoint.block_hash);
    }

    res.difficulty = m_core.get_blockchain_storage().get_difficulty_for_next_block();
    res.target = m_core.get_blockchain_storage().get_difficulty_target();
    res.tx_count = m_core.get_blockchain_storage().get_total_transactions() - res.height; //without coinbase
    res.tx_pool_size = m_core.get_pool().get_transactions_count();
    res.alt_blocks_count = restricted ? 0 : m_core.get_blockchain_storage().get_alternative_blocks_count();
    uint64_t total_conn = restricted ? 0 : m_p2p.get_public_connections_count();
    res.outgoing_connections_count = restricted ? 0 : m_p2p.get_public_outgoing_connections_count();
    res.incoming_connections_count = restricted ? 0 : (total_conn - res.outgoing_connections_count);
    res.rpc_connections_count = restricted ? 0 : get_connections_count();
    res.white_peerlist_size = restricted ? 0 : m_p2p.get_public_white_peers_count();
    res.grey_peerlist_size = restricted ? 0 : m_p2p.get_public_gray_peers_count();

    cryptonote::network_type nettype = m_core.get_nettype();
    res.mainnet = nettype == MAINNET;
    res.testnet = nettype == TESTNET;
    res.stagenet = nettype == STAGENET;
    res.nettype = nettype == MAINNET ? "mainnet" : nettype == TESTNET ? "testnet" : nettype == STAGENET ? "stagenet" : "fakechain";

    try
    {
      res.cumulative_difficulty = m_core.get_blockchain_storage().get_db().get_block_cumulative_difficulty(res.height - 1);
    }
    catch(std::exception const &e)
    {
      res.status = "Error retrieving cumulative difficulty at height " + std::to_string(res.height - 1);
      return true;
    }

    res.block_size_limit = res.block_weight_limit = m_core.get_blockchain_storage().get_current_cumulative_block_weight_limit();
    res.block_size_median = res.block_weight_median = m_core.get_blockchain_storage().get_current_cumulative_block_weight_median();
    res.start_time = restricted ? 0 : (uint64_t)m_core.get_start_time();
    res.last_storage_server_ping = restricted ? 0 : (uint64_t)m_core.m_last_storage_server_ping;
    res.last_lokinet_ping = restricted ? 0 : (uint64_t)m_core.m_last_lokinet_ping;
    res.free_space = restricted ? std::numeric_limits<uint64_t>::max() : m_core.get_free_space();
    res.offline = m_core.offline();
    res.bootstrap_daemon_address = restricted ? "" : m_bootstrap_daemon_address;
    res.height_without_bootstrap = restricted ? 0 : res.height;
    if (restricted)
      res.was_bootstrap_ever_used = false;
    else
    {
      boost::shared_lock<boost::shared_mutex> lock(m_bootstrap_daemon_mutex);
      res.was_bootstrap_ever_used = m_was_bootstrap_ever_used;
    }
    res.database_size = m_core.get_blockchain_storage().get_db().get_database_size();
    if (restricted)
      res.database_size = round_up(res.database_size, 5ull* 1024 * 1024 * 1024);
    res.update_available = restricted ? false : m_core.is_update_available();
    res.version = restricted ? std::to_string(LOKI_VERSION[0]) : LOKI_VERSION_STR;
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_net_stats(const COMMAND_RPC_GET_NET_STATS::request& req, COMMAND_RPC_GET_NET_STATS::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_net_stats);
    // No bootstrap daemon check: Only ever get stats about local server
    res.start_time = (uint64_t)m_core.get_start_time();
    {
      CRITICAL_REGION_LOCAL(epee::net_utils::network_throttle_manager::m_lock_get_global_throttle_in);
      epee::net_utils::network_throttle_manager::get_global_throttle_in().get_stats(res.total_packets_in, res.total_bytes_in);
    }
    {
      CRITICAL_REGION_LOCAL(epee::net_utils::network_throttle_manager::m_lock_get_global_throttle_out);
      epee::net_utils::network_throttle_manager::get_global_throttle_out().get_stats(res.total_packets_out, res.total_bytes_out);
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  class pruned_transaction {
    transaction& tx;
  public:
    pruned_transaction(transaction& tx) : tx(tx) {}
    BEGIN_SERIALIZE_OBJECT()
      bool r = tx.serialize_base(ar);
      if (!r) return false;
    END_SERIALIZE()
  };
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_blocks);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_BLOCKS_FAST>(invoke_http_mode::BIN, "/getblocks.bin", req, res, r))
      return r;

    std::vector<std::pair<std::pair<cryptonote::blobdata, crypto::hash>, std::vector<std::pair<crypto::hash, cryptonote::blobdata> > > > bs;

    if(!m_core.find_blockchain_supplement(req.start_height, req.block_ids, bs, res.current_height, res.start_height, req.prune, !req.no_miner_tx, COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT))
    {
      res.status = "Failed";
      return false;
    }

    size_t pruned_size = 0, unpruned_size = 0, ntxes = 0;
    res.blocks.reserve(bs.size());
    res.output_indices.reserve(bs.size());
    for(auto& bd: bs)
    {
      res.blocks.resize(res.blocks.size()+1);
      res.blocks.back().block = bd.first.first;
      pruned_size += bd.first.first.size();
      unpruned_size += bd.first.first.size();
      res.output_indices.push_back(COMMAND_RPC_GET_BLOCKS_FAST::block_output_indices());
      ntxes += bd.second.size();
      res.output_indices.back().indices.reserve(1 + bd.second.size());
      if (req.no_miner_tx)
        res.output_indices.back().indices.push_back(COMMAND_RPC_GET_BLOCKS_FAST::tx_output_indices());
      res.blocks.back().txs.reserve(bd.second.size());
      for (std::vector<std::pair<crypto::hash, cryptonote::blobdata>>::iterator i = bd.second.begin(); i != bd.second.end(); ++i)
      {
        unpruned_size += i->second.size();
        res.blocks.back().txs.push_back(std::move(i->second));
        i->second.clear();
        i->second.shrink_to_fit();
        pruned_size += res.blocks.back().txs.back().size();
      }

      const size_t n_txes_to_lookup = bd.second.size() + (req.no_miner_tx ? 0 : 1);
      if (n_txes_to_lookup > 0)
      {
        std::vector<std::vector<uint64_t>> indices;
        bool r = m_core.get_tx_outputs_gindexs(req.no_miner_tx ? bd.second.front().first : bd.first.second, n_txes_to_lookup, indices);
        if (!r)
        {
          res.status = "Failed";
          return false;
        }
        if (indices.size() != n_txes_to_lookup || res.output_indices.back().indices.size() != (req.no_miner_tx ? 1 : 0))
        {
          res.status = "Failed";
          return false;
        }
        for (size_t i = 0; i < indices.size(); ++i)
          res.output_indices.back().indices.push_back({std::move(indices[i])});
      }
    }

    MDEBUG("on_get_blocks: " << bs.size() << " blocks, " << ntxes << " txes, pruned size " << pruned_size << ", unpruned size " << unpruned_size);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
    bool core_rpc_server::on_get_alt_blocks_hashes(const COMMAND_RPC_GET_ALT_BLOCKS_HASHES::request& req, COMMAND_RPC_GET_ALT_BLOCKS_HASHES::response& res, const connection_context *ctx)
    {
      PERF_TIMER(on_get_alt_blocks_hashes);
      bool r;
      if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_ALT_BLOCKS_HASHES>(invoke_http_mode::JON, "/get_alt_blocks_hashes", req, res, r))
        return r;

      std::vector<block> blks;

      if(!m_core.get_alternative_blocks(blks))
      {
          res.status = "Failed";
          return false;
      }

      res.blks_hashes.reserve(blks.size());

      for (auto const& blk: blks)
      {
          res.blks_hashes.push_back(epee::string_tools::pod_to_hex(get_block_hash(blk)));
      }

      MDEBUG("on_get_alt_blocks_hashes: " << blks.size() << " blocks " );
      res.status = CORE_RPC_STATUS_OK;
      return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_blocks_by_height(const COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_blocks_by_height);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_BLOCKS_BY_HEIGHT>(invoke_http_mode::BIN, "/getblocks_by_height.bin", req, res, r))
      return r;

    res.status = "Failed";
    res.blocks.clear();
    res.blocks.reserve(req.heights.size());
    for (uint64_t height : req.heights)
    {
      block blk;
      try
      {
        blk = m_core.get_blockchain_storage().get_db().get_block_from_height(height);
      }
      catch (...)
      {
        res.status = "Error retrieving block at height " + std::to_string(height);
        return true;
      }
      std::vector<transaction> txs;
      std::vector<crypto::hash> missed_txs;
      m_core.get_transactions(blk.tx_hashes, txs, missed_txs);
      res.blocks.resize(res.blocks.size() + 1);
      res.blocks.back().block = block_to_blob(blk);
      for (auto& tx : txs)
        res.blocks.back().txs.push_back(tx_to_blob(tx));
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_hashes(const COMMAND_RPC_GET_HASHES_FAST::request& req, COMMAND_RPC_GET_HASHES_FAST::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_hashes);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_HASHES_FAST>(invoke_http_mode::BIN, "/gethashes.bin", req, res, r))
      return r;

    res.start_height = req.start_height;
    if(!m_core.get_blockchain_storage().find_blockchain_supplement(req.block_ids, res.m_block_ids, res.start_height, res.current_height, false))
    {
      res.status = "Failed";
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_outs_bin(const COMMAND_RPC_GET_OUTPUTS_BIN::request& req, COMMAND_RPC_GET_OUTPUTS_BIN::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_outs_bin);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_OUTPUTS_BIN>(invoke_http_mode::BIN, "/get_outs.bin", req, res, r))
      return r;

    res.status = "Failed";

    const bool restricted = m_restricted && ctx;
    if (restricted)
    {
      if (req.outputs.size() > MAX_RESTRICTED_GLOBAL_FAKE_OUTS_COUNT)
      {
        res.status = "Too many outs requested";
        return true;
      }
    }

    if(!m_core.get_outs(req, res))
    {
      return true;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_outs(const COMMAND_RPC_GET_OUTPUTS::request& req, COMMAND_RPC_GET_OUTPUTS::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_outs);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_OUTPUTS>(invoke_http_mode::JON, "/get_outs", req, res, r))
      return r;

    res.status = "Failed";

    const bool restricted = m_restricted && ctx;
    if (restricted)
    {
      if (req.outputs.size() > MAX_RESTRICTED_GLOBAL_FAKE_OUTS_COUNT)
      {
        res.status = "Too many outs requested";
        return true;
      }
    }

    cryptonote::COMMAND_RPC_GET_OUTPUTS_BIN::request req_bin;
    req_bin.outputs = req.outputs;
    req_bin.get_txid = req.get_txid;
    cryptonote::COMMAND_RPC_GET_OUTPUTS_BIN::response res_bin;
    if(!m_core.get_outs(req_bin, res_bin))
    {
      return true;
    }

    // convert to text
    for (const auto &i: res_bin.outs)
    {
      res.outs.push_back(cryptonote::COMMAND_RPC_GET_OUTPUTS::outkey());
      cryptonote::COMMAND_RPC_GET_OUTPUTS::outkey &outkey = res.outs.back();
      outkey.key = epee::string_tools::pod_to_hex(i.key);
      outkey.mask = epee::string_tools::pod_to_hex(i.mask);
      outkey.unlocked = i.unlocked;
      outkey.height = i.height;
      outkey.txid = epee::string_tools::pod_to_hex(i.txid);
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_indexes);
    bool ok;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES>(invoke_http_mode::BIN, "/get_o_indexes.bin", req, res, ok))
      return ok;

    bool r = m_core.get_tx_outputs_gindexs(req.txid, res.o_indexes);
    if(!r)
    {
      res.status = "Failed";
      return true;
    }
    res.status = CORE_RPC_STATUS_OK;
    LOG_PRINT_L2("COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES: [" << res.o_indexes.size() << "]");
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_transactions);
    bool ok;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_TRANSACTIONS>(invoke_http_mode::JON, "/gettransactions", req, res, ok))
      return ok;

    std::vector<crypto::hash> vh;
    for(const auto& tx_hex_str: req.txs_hashes)
    {
      blobdata b;
      if(!string_tools::parse_hexstr_to_binbuff(tx_hex_str, b))
      {
        res.status = "Failed to parse hex representation of transaction hash";
        return true;
      }
      if(b.size() != sizeof(crypto::hash))
      {
        res.status = "Failed, size of data mismatch";
        return true;
      }
      vh.push_back(*reinterpret_cast<const crypto::hash*>(b.data()));
    }
    std::vector<crypto::hash> missed_txs;
    std::vector<std::tuple<crypto::hash, cryptonote::blobdata, crypto::hash, cryptonote::blobdata>> txs;
    bool r = m_core.get_split_transactions_blobs(vh, txs, missed_txs);
    if(!r)
    {
      res.status = "Failed";
      return true;
    }
    LOG_PRINT_L2("Found " << txs.size() << "/" << vh.size() << " transactions on the blockchain");

    // try the pool for any missing txes
    auto &pool = m_core.get_pool();
    size_t found_in_pool = 0;
    std::unordered_map<crypto::hash, tx_info> per_tx_pool_tx_info;
    if (!missed_txs.empty())
    {
      std::vector<tx_info> pool_tx_info;
      std::vector<spent_key_image_info> pool_key_image_info;
      bool r = pool.get_transactions_and_spent_keys_info(pool_tx_info, pool_key_image_info);
      if(r)
      {
        // sort to match original request
        std::vector<std::tuple<crypto::hash, cryptonote::blobdata, crypto::hash, cryptonote::blobdata>> sorted_txs;
        unsigned txs_processed = 0;
        for (const crypto::hash &h: vh)
        {
          auto missed_it = std::find(missed_txs.begin(), missed_txs.end(), h);
          if (missed_it == missed_txs.end())
          {
            if (txs.size() == txs_processed)
            {
              res.status = "Failed: internal error - txs is empty";
              return true;
            }
            // core returns the ones it finds in the right order
            if (std::get<0>(txs[txs_processed]) != h)
            {
              res.status = "Failed: tx hash mismatch";
              return true;
            }
            sorted_txs.push_back(std::move(txs[txs_processed]));
            ++txs_processed;
            continue;
          }
          const std::string hash_string = epee::string_tools::pod_to_hex(h);
          auto ptx_it = std::find_if(pool_tx_info.begin(), pool_tx_info.end(),
              [&hash_string](const tx_info &txi) { return hash_string == txi.id_hash; });
          if (ptx_it != pool_tx_info.end())
          {
            cryptonote::transaction tx;
            if (!cryptonote::parse_and_validate_tx_from_blob(ptx_it->tx_blob, tx))
            {
              res.status = "Failed to parse and validate tx from blob";
              return true;
            }
            std::stringstream ss;
            binary_archive<true> ba(ss);
            if (!tx.serialize_base(ba))
            {
              res.status = "Failed to serialize transaction base";
              return true;
            }
            const cryptonote::blobdata pruned = ss.str();
            sorted_txs.emplace_back(h, pruned, get_transaction_prunable_hash(tx), std::string(ptx_it->tx_blob, pruned.size()));
            missed_txs.erase(missed_it);
            per_tx_pool_tx_info.emplace(h, *ptx_it);
            ++found_in_pool;
          }
        }
        txs = sorted_txs;
      }
      LOG_PRINT_L2("Found " << found_in_pool << "/" << vh.size() << " transactions in the pool");
    }

    uint64_t immutable_height = m_core.get_blockchain_storage().get_immutable_height();
    auto blink_lock = pool.blink_shared_lock(std::defer_lock); // Defer until/unless we actually need it

    std::vector<std::string>::const_iterator txhi = req.txs_hashes.begin();
    std::vector<crypto::hash>::const_iterator vhi = vh.begin();
    for(auto& tx: txs)
    {
      res.txs.emplace_back();
      COMMAND_RPC_GET_TRANSACTIONS::entry &e = res.txs.back();

      crypto::hash tx_hash = *vhi++;
      e.tx_hash = *txhi++;
      e.prunable_hash = epee::string_tools::pod_to_hex(std::get<2>(tx));
      if (req.split || req.prune || std::get<3>(tx).empty())
      {
        // use splitted form with pruned and prunable (filled only when prune=false and the daemon has it), leaving as_hex as empty
        e.pruned_as_hex = string_tools::buff_to_hex_nodelimer(std::get<1>(tx));
        if (!req.prune)
          e.prunable_as_hex = string_tools::buff_to_hex_nodelimer(std::get<3>(tx));
        if (req.decode_as_json)
        {
          cryptonote::blobdata tx_data;
          cryptonote::transaction t;
          if (req.prune || std::get<3>(tx).empty())
          {
            // decode pruned tx to JSON
            tx_data = std::get<1>(tx);
            if (cryptonote::parse_and_validate_tx_base_from_blob(tx_data, t))
            {
              pruned_transaction pruned_tx{t};
              e.as_json = obj_to_json_str(pruned_tx);
            }
            else
            {
              res.status = "Failed to parse and validate pruned tx from blob";
              return true;
            }
          }
          else
          {
            // decode full tx to JSON
            tx_data = std::get<1>(tx) + std::get<3>(tx);
            if (cryptonote::parse_and_validate_tx_from_blob(tx_data, t))
            {
              e.as_json = obj_to_json_str(t);
            }
            else
            {
              res.status = "Failed to parse and validate tx from blob";
              return true;
            }
          }
        }
      }
      else
      {
        // use non-splitted form, leaving pruned_as_hex and prunable_as_hex as empty
        cryptonote::blobdata tx_data = std::get<1>(tx) + std::get<3>(tx);
        e.as_hex = string_tools::buff_to_hex_nodelimer(tx_data);
        if (req.decode_as_json)
        {
          cryptonote::transaction t;
          if (cryptonote::parse_and_validate_tx_from_blob(tx_data, t))
          {
            e.as_json = obj_to_json_str(t);
          }
          else
          {
            res.status = "Failed to parse and validate tx from blob";
            return true;
          }
        }
      }
      auto ptx_it = per_tx_pool_tx_info.find(tx_hash);
      e.in_pool = ptx_it != per_tx_pool_tx_info.end();
      bool might_be_blink = true;
      if (e.in_pool)
      {
        e.block_height = e.block_timestamp = std::numeric_limits<uint64_t>::max();
        e.double_spend_seen = ptx_it->second.double_spend_seen;
        e.relayed = ptx_it->second.relayed;
      }
      else
      {
        e.block_height = m_core.get_blockchain_storage().get_db().get_tx_block_height(tx_hash);
        e.block_timestamp = m_core.get_blockchain_storage().get_db().get_block_timestamp(e.block_height);
        e.double_spend_seen = false;
        e.relayed = false;
        if (e.block_height <= immutable_height)
            might_be_blink = false;
      }

      if (might_be_blink)
      {
        if (!blink_lock) blink_lock.lock();
        e.blink = pool.has_blink(tx_hash);
      }

      // fill up old style responses too, in case an old wallet asks
      res.txs_as_hex.push_back(e.as_hex);
      if (req.decode_as_json)
        res.txs_as_json.push_back(e.as_json);

      // output indices too if not in pool
      if (!e.in_pool)
      {
        bool r = m_core.get_tx_outputs_gindexs(tx_hash, e.output_indices);
        if (!r)
        {
          res.status = "Failed";
          return false;
        }
      }
    }

    for(const auto& miss_tx: missed_txs)
    {
      res.missed_tx.push_back(string_tools::pod_to_hex(miss_tx));
    }

    LOG_PRINT_L2(res.txs.size() << " transactions found, " << res.missed_tx.size() << " not found");
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_is_key_image_spent(const COMMAND_RPC_IS_KEY_IMAGE_SPENT::request& req, COMMAND_RPC_IS_KEY_IMAGE_SPENT::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_is_key_image_spent);
    bool ok;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_IS_KEY_IMAGE_SPENT>(invoke_http_mode::JON, "/is_key_image_spent", req, res, ok))
      return ok;

    const bool restricted = m_restricted && ctx;
    const bool request_has_rpc_origin = ctx != NULL;
    std::vector<crypto::key_image> key_images;
    for(const auto& ki_hex_str: req.key_images)
    {
      blobdata b;
      if(!string_tools::parse_hexstr_to_binbuff(ki_hex_str, b))
      {
        res.status = "Failed to parse hex representation of key image";
        return true;
      }
      if(b.size() != sizeof(crypto::key_image))
      {
        res.status = "Failed, size of data mismatch";
      }
      key_images.push_back(*reinterpret_cast<const crypto::key_image*>(b.data()));
    }
    std::vector<bool> spent_status;
    bool r = m_core.are_key_images_spent(key_images, spent_status);
    if(!r)
    {
      res.status = "Failed";
      return true;
    }
    res.spent_status.clear();
    for (size_t n = 0; n < spent_status.size(); ++n)
      res.spent_status.push_back(spent_status[n] ? COMMAND_RPC_IS_KEY_IMAGE_SPENT::SPENT_IN_BLOCKCHAIN : COMMAND_RPC_IS_KEY_IMAGE_SPENT::UNSPENT);

    // check the pool too
    std::vector<cryptonote::tx_info> txs;
    std::vector<cryptonote::spent_key_image_info> ki;
    r = m_core.get_pool().get_transactions_and_spent_keys_info(txs, ki, !request_has_rpc_origin || !restricted);
    if(!r)
    {
      res.status = "Failed";
      return true;
    }
    for (std::vector<cryptonote::spent_key_image_info>::const_iterator i = ki.begin(); i != ki.end(); ++i)
    {
      crypto::hash hash;
      crypto::key_image spent_key_image;
      if (parse_hash256(i->id_hash, hash))
      {
        memcpy(&spent_key_image, &hash, sizeof(hash)); // a bit dodgy, should be other parse functions somewhere
        for (size_t n = 0; n < res.spent_status.size(); ++n)
        {
          if (res.spent_status[n] == COMMAND_RPC_IS_KEY_IMAGE_SPENT::UNSPENT)
          {
            if (key_images[n] == spent_key_image)
            {
              res.spent_status[n] = COMMAND_RPC_IS_KEY_IMAGE_SPENT::SPENT_IN_POOL;
              break;
            }
          }
        }
      }
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_send_raw_tx(const COMMAND_RPC_SEND_RAW_TX::request& req, COMMAND_RPC_SEND_RAW_TX::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_send_raw_tx);
    bool ok;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_SEND_RAW_TX>(invoke_http_mode::JON, "/sendrawtransaction", req, res, ok))
      return ok;

    CHECK_CORE_READY();

    std::string tx_blob;
    if(!string_tools::parse_hexstr_to_binbuff(req.tx_as_hex, tx_blob))
    {
      LOG_PRINT_L0("[on_send_raw_tx]: Failed to parse tx from hexbuff: " << req.tx_as_hex);
      res.status = "Failed";
      return true;
    }

    if (req.do_sanity_checks && !cryptonote::tx_sanity_check(m_core.get_blockchain_storage(), tx_blob))
    {
      res.status = "Failed";
      res.reason = "Sanity check failed";
      res.sanity_check_failed = true;
      return true;
    }
    res.sanity_check_failed = false;

    if (req.blink)
    {
      using namespace std::chrono_literals;
      auto future = m_core.handle_blink_tx(tx_blob);
      auto status = future.wait_for(10s);
      if (status != std::future_status::ready) {
        res.status = "Failed";
        res.reason = "Blink quorum timeout";
        res.blink_status = blink_result::timeout;
        return true;
      }

      try {
        auto result = future.get();
        res.blink_status = result.first;
        if (result.first == blink_result::accepted) {
          res.status = CORE_RPC_STATUS_OK;
        } else {
          res.status = "Failed";
          res.reason = !result.second.empty() ? result.second : result.first == blink_result::timeout ? "Blink quorum timeout" : "Transaction rejected by blink quorum";
        }
      } catch (const std::exception &e) {
        res.blink_status = blink_result::rejected;
        res.status = "Failed";
        res.reason = std::string{"Transaction failed: "} + e.what();
      }
      return true;
    }

    tx_verification_context tvc{};
    if(!m_core.handle_incoming_tx(tx_blob, tvc, tx_pool_options::new_tx(req.do_not_relay)) || tvc.m_verifivation_failed)
    {
      const vote_verification_context &vvc = tvc.m_vote_ctx;
      res.status          = "Failed";
      std::string reason  = print_tx_verification_context  (tvc);
      reason             += print_vote_verification_context(vvc);
      res.tvc             = tvc;
      const std::string punctuation = res.reason.empty() ? "" : ": ";
      if (tvc.m_verifivation_failed)
      {
        LOG_PRINT_L0("[on_send_raw_tx]: tx verification failed" << punctuation << reason);
      }
      else
      {
        LOG_PRINT_L0("[on_send_raw_tx]: Failed to process tx" << punctuation << reason);
      }
      return true;
    }

    if(!tvc.m_should_be_relayed)
    {
      LOG_PRINT_L0("[on_send_raw_tx]: tx accepted, but not relayed");
      res.reason = "Not relayed";
      res.not_relayed = true;
      res.status = CORE_RPC_STATUS_OK;
      return true;
    }

    NOTIFY_NEW_TRANSACTIONS::request r;
    r.txs.push_back(tx_blob);
    cryptonote_connection_context fake_context{};
    m_core.get_protocol()->relay_transactions(r, fake_context);

    //TODO: make sure that tx has reached other nodes here, probably wait to receive reflections from other nodes
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_start_mining(const COMMAND_RPC_START_MINING::request& req, COMMAND_RPC_START_MINING::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_start_mining);
    CHECK_CORE_READY();
    cryptonote::address_parse_info info;
    if(!get_account_address_from_str(info, m_core.get_nettype(), req.miner_address))
    {
      res.status = "Failed, wrong address";
      LOG_PRINT_L0(res.status);
      return true;
    }
    if (info.is_subaddress)
    {
      res.status = "Mining to subaddress isn't supported yet";
      LOG_PRINT_L0(res.status);
      return true;
    }

    unsigned int concurrency_count = boost::thread::hardware_concurrency() * 4;

    // if we couldn't detect threads, set it to a ridiculously high number
    if(concurrency_count == 0)
    {
      concurrency_count = 257;
    }

    // if there are more threads requested than the hardware supports
    // then we fail and log that.
    if(req.threads_count > concurrency_count)
    {
      res.status = "Failed, too many threads relative to CPU cores.";
      LOG_PRINT_L0(res.status);
      return true;
    }

    cryptonote::miner &miner= m_core.get_miner();
    if (miner.is_mining())
    {
      res.status = "Already mining";
      return true;
    }
    if(!miner.start(info.address, static_cast<size_t>(req.threads_count), req.do_background_mining, req.ignore_battery, req.num_blocks, req.slow_mining))
    {
      res.status = "Failed, mining not started";
      LOG_PRINT_L0(res.status);
      return true;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_stop_mining(const COMMAND_RPC_STOP_MINING::request& req, COMMAND_RPC_STOP_MINING::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_stop_mining);
    cryptonote::miner &miner= m_core.get_miner();
    if(!miner.is_mining())
    {
      res.status = "Mining never started";
      LOG_PRINT_L0(res.status);
      return true;
    }
    if(!miner.stop())
    {
      res.status = "Failed, mining not stopped";
      LOG_PRINT_L0(res.status);
      return true;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_mining_status(const COMMAND_RPC_MINING_STATUS::request& req, COMMAND_RPC_MINING_STATUS::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_mining_status);

    const miner& lMiner = m_core.get_miner();
    res.active = lMiner.is_mining();
    res.is_background_mining_enabled = lMiner.get_is_background_mining_enabled();
    res.block_target = DIFFICULTY_TARGET_V2;
    res.difficulty = m_core.get_blockchain_storage().get_difficulty_for_next_block();
    if ( lMiner.is_mining() ) {
      res.speed = lMiner.get_speed();
      res.threads_count = lMiner.get_threads_count();
      res.block_reward = lMiner.get_block_reward();
    }
    const account_public_address& lMiningAdr = lMiner.get_mining_address();
    res.address = get_account_address_as_str(nettype(), false, lMiningAdr);
    const uint8_t major_version = m_core.get_blockchain_storage().get_current_hard_fork_version();

    res.pow_algorithm =
        major_version >= network_version_12_checkpointing    ? "RandomX (LOKI variant)"               :
        major_version == network_version_11_infinite_staking ? "Cryptonight Turtle Light (Variant 2)" :
                                                               "Cryptonight Heavy (Variant 2)";

    if (res.is_background_mining_enabled)
    {
      res.bg_idle_threshold = lMiner.get_idle_threshold();
      res.bg_min_idle_seconds = lMiner.get_min_idle_seconds();
      res.bg_ignore_battery = lMiner.get_ignore_battery();
      res.bg_target = lMiner.get_mining_target();
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_save_bc(const COMMAND_RPC_SAVE_BC::request& req, COMMAND_RPC_SAVE_BC::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_save_bc);
    if( !m_core.get_blockchain_storage().store_blockchain() )
    {
      res.status = "Error while storing blockchain";
      return true;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_peer_list(const COMMAND_RPC_GET_PEER_LIST::request& req, COMMAND_RPC_GET_PEER_LIST::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_peer_list);
    std::vector<nodetool::peerlist_entry> white_list;
    std::vector<nodetool::peerlist_entry> gray_list;
    m_p2p.get_public_peerlist(gray_list, white_list);

    res.white_list.reserve(white_list.size());
    for (auto & entry : white_list)
    {
      if (entry.adr.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id())
        res.white_list.emplace_back(entry.id, entry.adr.as<epee::net_utils::ipv4_network_address>().ip(),
            entry.adr.as<epee::net_utils::ipv4_network_address>().port(), entry.last_seen, entry.pruning_seed, entry.rpc_port);
      else if (entry.adr.get_type_id() == epee::net_utils::ipv6_network_address::get_type_id())
      {
        res.white_list.emplace_back(entry.id, entry.adr.as<epee::net_utils::ipv6_network_address>().host_str(),
            entry.adr.as<epee::net_utils::ipv6_network_address>().port(), entry.last_seen, entry.pruning_seed, entry.rpc_port);
      }
      else
        res.white_list.emplace_back(entry.id, entry.adr.str(), entry.last_seen, entry.pruning_seed, entry.rpc_port);
    }

    res.gray_list.reserve(gray_list.size());
    for (auto & entry : gray_list)
    {
      if (entry.adr.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id())
        res.gray_list.emplace_back(entry.id, entry.adr.as<epee::net_utils::ipv4_network_address>().ip(),
            entry.adr.as<epee::net_utils::ipv4_network_address>().port(), entry.last_seen, entry.pruning_seed, entry.rpc_port);
      else if (entry.adr.get_type_id() == epee::net_utils::ipv6_network_address::get_type_id())
        res.white_list.emplace_back(entry.id, entry.adr.as<epee::net_utils::ipv6_network_address>().host_str(),
            entry.adr.as<epee::net_utils::ipv6_network_address>().port(), entry.last_seen, entry.pruning_seed, entry.rpc_port);
      else
        res.gray_list.emplace_back(entry.id, entry.adr.str(), entry.last_seen, entry.pruning_seed, entry.rpc_port);
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_set_log_hash_rate(const COMMAND_RPC_SET_LOG_HASH_RATE::request& req, COMMAND_RPC_SET_LOG_HASH_RATE::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_set_log_hash_rate);
    if(m_core.get_miner().is_mining())
    {
      m_core.get_miner().do_print_hashrate(req.visible);
      res.status = CORE_RPC_STATUS_OK;
    }
    else
    {
      res.status = CORE_RPC_STATUS_NOT_MINING;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_set_log_level(const COMMAND_RPC_SET_LOG_LEVEL::request& req, COMMAND_RPC_SET_LOG_LEVEL::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_set_log_level);
    if (req.level < 0 || req.level > 4)
    {
      res.status = "Error: log level not valid";
      return true;
    }
    mlog_set_log_level(req.level);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_set_log_categories(const COMMAND_RPC_SET_LOG_CATEGORIES::request& req, COMMAND_RPC_SET_LOG_CATEGORIES::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_set_log_categories);
    mlog_set_log(req.categories.c_str());
    res.categories = mlog_get_categories();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_transaction_pool(const COMMAND_RPC_GET_TRANSACTION_POOL::request& req, COMMAND_RPC_GET_TRANSACTION_POOL::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_transaction_pool);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_TRANSACTION_POOL>(invoke_http_mode::JON, "/get_transaction_pool", req, res, r))
      return r;

    const bool restricted = m_restricted && ctx;
    const bool request_has_rpc_origin = ctx != NULL;
    m_core.get_pool().get_transactions_and_spent_keys_info(res.transactions, res.spent_key_images, !request_has_rpc_origin || !restricted);
    for (tx_info& txi : res.transactions)
      txi.tx_blob = epee::string_tools::buff_to_hex_nodelimer(txi.tx_blob);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_transaction_pool_hashes_bin(const COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::request& req, COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_transaction_pool_hashes);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN>(invoke_http_mode::JON, "/get_transaction_pool_hashes.bin", req, res, r))
      return r;

    std::vector<crypto::hash> tx_pool_hashes;
    const bool restricted = m_restricted && ctx;
    const bool request_has_rpc_origin = ctx != NULL;
    m_core.get_pool().get_transaction_hashes(tx_pool_hashes, !request_has_rpc_origin || !restricted);

    if (req.long_poll)
    {
      if (m_max_long_poll_connections <= 0)
      {
        // Essentially disable long polling by making the wallet long polling thread go to sleep due to receiving this message
        res.status = CORE_RPC_STATUS_TX_LONG_POLL_MAX_CONNECTIONS;
        return true;
      }

      crypto::hash checksum = {};
      for (crypto::hash const &hash : tx_pool_hashes) crypto::hash_xor(checksum, hash);

      if (req.tx_pool_checksum == checksum)
      {
        size_t tx_count_before = tx_pool_hashes.size();
        time_t before          = time(nullptr);
        std::unique_lock<std::mutex> lock(m_core.m_long_poll_mutex);
        if ((m_long_poll_active_connections + 1) > m_max_long_poll_connections)
        {
          res.status = CORE_RPC_STATUS_TX_LONG_POLL_MAX_CONNECTIONS;
          return true;
        }

        m_long_poll_active_connections++;
        bool condition_activated = m_core.m_long_poll_wake_up_clients.wait_for(lock, rpc_long_poll_timeout, [this, tx_count_before]() {
              size_t tx_count_after = m_core.get_pool().get_transactions_count();
              return tx_count_before != tx_count_after;
            });

        m_long_poll_active_connections--;
        if (!condition_activated)
        {
          res.status = CORE_RPC_STATUS_TX_LONG_POLL_TIMED_OUT;
          return true;
        }
      }
    }

    res.tx_hashes = std::move(tx_pool_hashes);
    res.status    = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_transaction_pool_hashes(const COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::request& req, COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_transaction_pool_hashes);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_TRANSACTION_POOL_HASHES>(invoke_http_mode::JON, "/get_transaction_pool_hashes", req, res, r))
      return r;

    const bool restricted = m_restricted && ctx;
    const bool request_has_rpc_origin = ctx != NULL;
    std::vector<crypto::hash> tx_hashes;
    m_core.get_pool().get_transaction_hashes(tx_hashes, !request_has_rpc_origin || !restricted);
    res.tx_hashes.reserve(tx_hashes.size());
    for (const crypto::hash &tx_hash: tx_hashes)
      res.tx_hashes.push_back(epee::string_tools::pod_to_hex(tx_hash));
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_transaction_pool_stats(const COMMAND_RPC_GET_TRANSACTION_POOL_STATS::request& req, COMMAND_RPC_GET_TRANSACTION_POOL_STATS::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_transaction_pool_stats);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_TRANSACTION_POOL_STATS>(invoke_http_mode::JON, "/get_transaction_pool_stats", req, res, r))
      return r;

    const bool restricted = m_restricted && ctx;
    const bool request_has_rpc_origin = ctx != NULL;
    m_core.get_pool().get_transaction_stats(res.pool_stats, !request_has_rpc_origin || !restricted);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_stop_daemon(const COMMAND_RPC_STOP_DAEMON::request& req, COMMAND_RPC_STOP_DAEMON::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_stop_daemon);
    // FIXME: replace back to original m_p2p.send_stop_signal() after
    // investigating why that isn't working quite right.
    m_p2p.send_stop_signal();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------

  //
  // Loki
  //
  bool core_rpc_server::on_get_output_blacklist_bin(const COMMAND_RPC_GET_OUTPUT_BLACKLIST::request& req, COMMAND_RPC_GET_OUTPUT_BLACKLIST::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_output_blacklist_bin);

    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_OUTPUT_BLACKLIST>(invoke_http_mode::BIN, "/get_output_blacklist.bin", req, res, r))
      return r;

    res.status = "Failed";
    try
    {
      m_core.get_output_blacklist(res.blacklist);
    }
    catch (const std::exception &e)
    {
      res.status = "Failed to get output blacklist";
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_getblockcount);
    {
      boost::shared_lock<boost::shared_mutex> lock(m_bootstrap_daemon_mutex);
      if (m_should_use_bootstrap_daemon)
      {
        res.status = "This command is unsupported for bootstrap daemon";
        return false;
      }
    }
    res.count = m_core.get_current_blockchain_height();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request& req, COMMAND_RPC_GETBLOCKHASH::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_getblockhash);
    {
      boost::shared_lock<boost::shared_mutex> lock(m_bootstrap_daemon_mutex);
      if (m_should_use_bootstrap_daemon)
      {
        res = "This command is unsupported for bootstrap daemon";
        return false;
      }
    }
    if(req.size() != 1)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Wrong parameters, expected height";
      return false;
    }
    uint64_t h = req[0];
    if(m_core.get_current_blockchain_height() <= h)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
      error_resp.message = std::string("Requested block height: ") + std::to_string(h) + " greater than current top block height: " +  std::to_string(m_core.get_current_blockchain_height() - 1);
    }
    res = string_tools::pod_to_hex(m_core.get_block_id_by_height(h));
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  // equivalent of strstr, but with arbitrary bytes (ie, NULs)
  // This does not differentiate between "not found" and "found at offset 0"
  size_t slow_memmem(const void* start_buff, size_t buflen,const void* pat,size_t patlen)
  {
    const void* buf = start_buff;
    const void* end=(const char*)buf+buflen;
    if (patlen > buflen || patlen == 0) return 0;
    while(buflen>0 && (buf=memchr(buf,((const char*)pat)[0],buflen-patlen+1)))
    {
      if(memcmp(buf,pat,patlen)==0)
        return (const char*)buf - (const char*)start_buff;
      buf=(const char*)buf+1;
      buflen = (const char*)end - (const char*)buf;
    }
    return 0;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_getblocktemplate);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GETBLOCKTEMPLATE>(invoke_http_mode::JON_RPC, "getblocktemplate", req, res, r))
      return r;

    if(!check_core_ready())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_CORE_BUSY;
      error_resp.message = "Core is busy";
      return false;
    }

    if(req.reserve_size > 255)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE;
      error_resp.message = "Too big reserved size, maximum 255";
      return false;
    }

    cryptonote::address_parse_info info;

    if(!req.wallet_address.size() || !cryptonote::get_account_address_from_str(info, m_core.get_nettype(), req.wallet_address))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS;
      error_resp.message = "Failed to parse wallet address";
      return false;
    }
    if (info.is_subaddress)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_MINING_TO_SUBADDRESS;
      error_resp.message = "Mining to subaddress is not supported yet";
      return false;
    }

    block b;
    cryptonote::blobdata blob_reserve;
    blob_reserve.resize(req.reserve_size, 0);
    cryptonote::difficulty_type diff;
    crypto::hash prev_block;
    if (!req.prev_block.empty())
    {
      if (!epee::string_tools::hex_to_pod(req.prev_block, prev_block))
      {
        error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
        error_resp.message = "Invalid prev_block";
        return false;
      }
    }
    if(!m_core.get_block_template(b, req.prev_block.empty() ? NULL : &prev_block, info.address, diff, res.height, res.expected_reward, blob_reserve))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: failed to create block template";
      LOG_ERROR("Failed to create block template");
      return false;
    }

    if (b.major_version >= network_version_19_checkpointing)
    {
      uint64_t seed_height, next_height;
      crypto::hash seed_hash;
      crypto::rx_seedheights(res.height, &seed_height, &next_height);
      seed_hash = m_core.get_block_id_by_height(seed_height);
      res.seed_hash = string_tools::pod_to_hex(seed_hash);
      if (next_height != seed_height) {
        seed_hash = m_core.get_block_id_by_height(next_height);
        res.next_seed_hash = string_tools::pod_to_hex(seed_hash);
      }
    }
    res.difficulty = diff;

    blobdata block_blob = t_serializable_object_to_blob(b);
    crypto::public_key tx_pub_key = cryptonote::get_tx_pub_key_from_extra(b.miner_tx);
    if(tx_pub_key == crypto::null_pkey)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: failed to create block template";
      LOG_ERROR("Failed to get tx pub key in coinbase extra");
      return false;
    }
    res.reserved_offset = slow_memmem((void*)block_blob.data(), block_blob.size(), &tx_pub_key, sizeof(tx_pub_key));
    if(!res.reserved_offset)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: failed to create block template";
      LOG_ERROR("Failed to find tx pub key in blockblob");
      return false;
    }
    if (req.reserve_size)
      res.reserved_offset += sizeof(tx_pub_key) + 2; //2 bytes: tag for TX_EXTRA_NONCE(1 byte), counter in TX_EXTRA_NONCE(1 byte)
    else
      res.reserved_offset = 0;
    if(res.reserved_offset + req.reserve_size > block_blob.size())
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: failed to create block template";
      LOG_ERROR("Failed to calculate offset for ");
      return false;
    }
    blobdata hashing_blob = get_block_hashing_blob(b);
    res.prev_hash = string_tools::pod_to_hex(b.prev_id);
    res.blocktemplate_blob = string_tools::buff_to_hex_nodelimer(block_blob);
    res.blockhashing_blob =  string_tools::buff_to_hex_nodelimer(hashing_blob);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_submitblock);
    {
      boost::shared_lock<boost::shared_mutex> lock(m_bootstrap_daemon_mutex);
      if (m_should_use_bootstrap_daemon)
      {
        res.status = "This command is unsupported for bootstrap daemon";
        return false;
      }
    }
    CHECK_CORE_READY();
    if(req.size()!=1)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Wrong param";
      return false;
    }
    blobdata blockblob;
    if(!string_tools::parse_hexstr_to_binbuff(req[0], blockblob))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB;
      error_resp.message = "Wrong block blob";
      return false;
    }

    // Fixing of high orphan issue for most pools
    // Thanks Boolberry!
    block b;
    if(!parse_and_validate_block_from_blob(blockblob, b))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB;
      error_resp.message = "Wrong block blob";
      return false;
    }

    // Fix from Boolberry neglects to check block
    // size, do that with the function below
    if(!m_core.check_incoming_block_size(blockblob))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB_SIZE;
      error_resp.message = "Block bloc size is too big, rejecting block";
      return false;
    }

    block_verification_context bvc;
    if(!m_core.handle_block_found(b, bvc))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED;
      error_resp.message = "Block not accepted";
      return false;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_generateblocks(const COMMAND_RPC_GENERATEBLOCKS::request& req, COMMAND_RPC_GENERATEBLOCKS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_generateblocks);

    CHECK_CORE_READY();

    res.status = CORE_RPC_STATUS_OK;

    if(m_core.get_nettype() != FAKECHAIN)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_REGTEST_REQUIRED;
      error_resp.message = "Regtest required when generating blocks";
      return false;
    }

    COMMAND_RPC_GETBLOCKTEMPLATE::request template_req;
    COMMAND_RPC_GETBLOCKTEMPLATE::response template_res;
    COMMAND_RPC_SUBMITBLOCK::request submit_req;
    COMMAND_RPC_SUBMITBLOCK::response submit_res;

    template_req.reserve_size = 1;
    template_req.wallet_address = req.wallet_address;
    template_req.prev_block = req.prev_block;
    submit_req.emplace_back();
    res.height = m_core.get_blockchain_storage().get_current_blockchain_height();

    for(size_t i = 0; i < req.amount_of_blocks; i++)
    {
      bool r = on_getblocktemplate(template_req, template_res, error_resp, ctx);
      res.status = template_res.status;
      template_req.prev_block.clear();
      
      if (!r) return false;

      blobdata blockblob;
      if(!string_tools::parse_hexstr_to_binbuff(template_res.blocktemplate_blob, blockblob))
      {
        error_resp.code = CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB;
        error_resp.message = "Wrong block blob";
        return false;
      }
      block b;
      if(!parse_and_validate_block_from_blob(blockblob, b))
      {
        error_resp.code = CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB;
        error_resp.message = "Wrong block blob";
        return false;
      }
      b.nonce = req.starting_nonce;
      miner::find_nonce_for_given_block(&(m_core.get_blockchain_storage()), b, template_res.difficulty, template_res.height);

      submit_req.front() = string_tools::buff_to_hex_nodelimer(block_to_blob(b));
      r = on_submitblock(submit_req, submit_res, error_resp, ctx);
      res.status = submit_res.status;

      if (!r) return false;

      res.blocks.push_back(epee::string_tools::pod_to_hex(get_block_hash(b)));
      template_req.prev_block = res.blocks.back();
      res.height = template_res.height;
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  uint64_t core_rpc_server::get_block_reward(const block& blk)
  {
    uint64_t reward = 0;
    for(const tx_out& out: blk.miner_tx.vout)
    {
      reward += out.amount;
    }
    return reward;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::fill_block_header_response(const block& blk, bool orphan_status, uint64_t height, const crypto::hash& hash, block_header_response& response, bool fill_pow_hash)
  {
    PERF_TIMER(fill_block_header_response);
    response.major_version = blk.major_version;
    response.minor_version = blk.minor_version;
    response.timestamp = blk.timestamp;
    response.prev_hash = string_tools::pod_to_hex(blk.prev_id);
    response.nonce = blk.nonce;
    response.orphan_status = orphan_status;
    response.height = height;
    response.depth = m_core.get_current_blockchain_height() - height - 1;
    response.hash = string_tools::pod_to_hex(hash);
    response.difficulty = m_core.get_blockchain_storage().block_difficulty(height);
    response.cumulative_difficulty = m_core.get_blockchain_storage().get_db().get_block_cumulative_difficulty(height);
    response.block_weight = m_core.get_blockchain_storage().get_db().get_block_weight(height);
    response.reward = get_block_reward(blk);
    response.miner_reward = blk.miner_tx.vout[0].amount;
    response.block_size = response.block_weight = m_core.get_blockchain_storage().get_db().get_block_weight(height);
    response.num_txes = blk.tx_hashes.size();
    response.pow_hash = fill_pow_hash ? string_tools::pod_to_hex(get_block_longhash(&(m_core.get_blockchain_storage()), blk, height, 0)) : "";
    response.long_term_weight = m_core.get_blockchain_storage().get_db().get_block_long_term_weight(height);
    response.miner_tx_hash = string_tools::pod_to_hex(cryptonote::get_transaction_hash(blk.miner_tx));
    response.service_node_winner = string_tools::pod_to_hex(cryptonote::get_service_node_winner_from_tx_extra(blk.miner_tx.extra));
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  template <typename COMMAND_TYPE>
  bool core_rpc_server::use_bootstrap_daemon_if_necessary(const invoke_http_mode &mode, const std::string &command_name, const typename COMMAND_TYPE::request& req, typename COMMAND_TYPE::response& res, bool &r)
  {
    res.untrusted = false;
    if (m_bootstrap_daemon_address.empty())
      return false;

    boost::unique_lock<boost::shared_mutex> lock(m_bootstrap_daemon_mutex);
    if (!m_should_use_bootstrap_daemon)
    {
      MINFO("The local daemon is fully synced. Not switching back to the bootstrap daemon");
      return false;
    }

    auto current_time = std::chrono::system_clock::now();
    if (current_time - m_bootstrap_height_check_time > std::chrono::seconds(30))  // update every 30s
    {
      m_bootstrap_height_check_time = current_time;

      uint64_t top_height;
      crypto::hash top_hash;
      m_core.get_blockchain_top(top_height, top_hash);
      ++top_height; // turn top block height into blockchain height

      // query bootstrap daemon's height
      cryptonote::COMMAND_RPC_GET_HEIGHT::request getheight_req;
      cryptonote::COMMAND_RPC_GET_HEIGHT::response getheight_res;
      bool ok = epee::net_utils::invoke_http_json("/getheight", getheight_req, getheight_res, m_http_client);
      ok = ok && getheight_res.status == CORE_RPC_STATUS_OK;

      m_should_use_bootstrap_daemon = ok && top_height + 10 < getheight_res.height;
      MINFO((m_should_use_bootstrap_daemon ? "Using" : "Not using") << " the bootstrap daemon (our height: " << top_height << ", bootstrap daemon's height: " << getheight_res.height << ")");
    }
    if (!m_should_use_bootstrap_daemon)
      return false;

    if (mode == invoke_http_mode::JON)
    {
      r = epee::net_utils::invoke_http_json(command_name, req, res, m_http_client);
    }
    else if (mode == invoke_http_mode::BIN)
    {
      r = epee::net_utils::invoke_http_bin(command_name, req, res, m_http_client);
    }
    else if (mode == invoke_http_mode::JON_RPC)
    {
      epee::json_rpc::request<typename COMMAND_TYPE::request> json_req{};
      epee::json_rpc::response<typename COMMAND_TYPE::response, std::string> json_resp{};
      json_req.jsonrpc = "2.0";
      json_req.id = epee::serialization::storage_entry(0);
      json_req.method = command_name;
      json_req.params = req;
      r = net_utils::invoke_http_json("/json_rpc", json_req, json_resp, m_http_client);
      if (r)
        res = json_resp.result;
    }
    else
    {
      MERROR("Unknown invoke_http_mode: " << mode);
      return false;
    }
    m_was_bootstrap_ever_used = true;
    r = r && res.status == CORE_RPC_STATUS_OK;
    res.untrusted = true;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_last_block_header);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_LAST_BLOCK_HEADER>(invoke_http_mode::JON_RPC, "getlastblockheader", req, res, r))
      return r;

    CHECK_CORE_READY();
    uint64_t last_block_height;
    crypto::hash last_block_hash;
    m_core.get_blockchain_top(last_block_height, last_block_hash);
    block last_block;
    bool have_last_block = m_core.get_block_by_hash(last_block_hash, last_block);
    if (!have_last_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: can't get last block.";
      return false;
    }
    const bool restricted = m_restricted && ctx;
    bool response_filled = fill_block_header_response(last_block, false, last_block_height, last_block_hash, res.block_header, req.fill_pow_hash && !restricted);
    if (!response_filled)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: can't produce valid response.";
      return false;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_block_header_by_hash);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH>(invoke_http_mode::JON_RPC, "getblockheaderbyhash", req, res, r))
      return r;

    crypto::hash block_hash;
    bool hash_parsed = parse_hash256(req.hash, block_hash);
    if(!hash_parsed)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Failed to parse hex representation of block hash. Hex = " + req.hash + '.';
      return false;
    }
    block blk;
    bool orphan = false;
    bool have_block = m_core.get_block_by_hash(block_hash, blk, &orphan);
    if (!have_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: can't get block by hash. Hash = " + req.hash + '.';
      return false;
    }
    if (blk.miner_tx.vin.size() != 1 || blk.miner_tx.vin.front().type() != typeid(txin_gen))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: coinbase transaction in the block has the wrong type";
      return false;
    }
    uint64_t block_height = boost::get<txin_gen>(blk.miner_tx.vin.front()).height;
    const bool restricted = m_restricted && ctx;
    bool response_filled = fill_block_header_response(blk, orphan, block_height, block_hash, res.block_header, req.fill_pow_hash && !restricted);
    if (!response_filled)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: can't produce valid response.";
      return false;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_block_headers_range(const COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::request& req, COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_block_headers_range);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_BLOCK_HEADERS_RANGE>(invoke_http_mode::JON_RPC, "getblockheadersrange", req, res, r))
      return r;

    const uint64_t bc_height = m_core.get_current_blockchain_height();
    if (req.start_height >= bc_height || req.end_height >= bc_height || req.start_height > req.end_height)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
      error_resp.message = "Invalid start/end heights.";
      return false;
    }
    for (uint64_t h = req.start_height; h <= req.end_height; ++h)
    {
      crypto::hash block_hash = m_core.get_block_id_by_height(h);
      block blk;
      bool have_block = m_core.get_block_by_hash(block_hash, blk);
      if (!have_block)
      {
        error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
        error_resp.message = "Internal error: can't get block by height. Height = " + boost::lexical_cast<std::string>(h) + ". Hash = " + epee::string_tools::pod_to_hex(block_hash) + '.';
        return false;
      }
      if (blk.miner_tx.vin.size() != 1 || blk.miner_tx.vin.front().type() != typeid(txin_gen))
      {
        error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
        error_resp.message = "Internal error: coinbase transaction in the block has the wrong type";
        return false;
      }
      uint64_t block_height = boost::get<txin_gen>(blk.miner_tx.vin.front()).height;
      if (block_height != h)
      {
        error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
        error_resp.message = "Internal error: coinbase transaction in the block has the wrong height";
        return false;
      }
      res.headers.push_back(block_header_response());
      const bool restricted = m_restricted && ctx;
      bool response_filled = fill_block_header_response(blk, false, block_height, block_hash, res.headers.back(), req.fill_pow_hash && !restricted);
      if (!response_filled)
      {
        error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
        error_resp.message = "Internal error: can't produce valid response.";
        return false;
      }
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_block_header_by_height);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT>(invoke_http_mode::JON_RPC, "getblockheaderbyheight", req, res, r))
      return r;

    if(m_core.get_current_blockchain_height() <= req.height)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
      error_resp.message = std::string("Requested block height: ") + std::to_string(req.height) + " greater than current top block height: " +  std::to_string(m_core.get_current_blockchain_height() - 1);
      return false;
    }
    crypto::hash block_hash = m_core.get_block_id_by_height(req.height);
    block blk;
    bool have_block = m_core.get_block_by_hash(block_hash, blk);
    if (!have_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: can't get block by height. Height = " + std::to_string(req.height) + '.';
      return false;
    }
    const bool restricted = m_restricted && ctx;
    bool response_filled = fill_block_header_response(blk, false, req.height, block_hash, res.block_header, req.fill_pow_hash && !restricted);
    if (!response_filled)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: can't produce valid response.";
      return false;
    }
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_block(const COMMAND_RPC_GET_BLOCK::request& req, COMMAND_RPC_GET_BLOCK::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_block);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_BLOCK>(invoke_http_mode::JON_RPC, "getblock", req, res, r))
      return r;

    crypto::hash block_hash;
    if (!req.hash.empty())
    {
      bool hash_parsed = parse_hash256(req.hash, block_hash);
      if(!hash_parsed)
      {
        error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
        error_resp.message = "Failed to parse hex representation of block hash. Hex = " + req.hash + '.';
        return false;
      }
    }
    else
    {
      if(m_core.get_current_blockchain_height() <= req.height)
      {
        error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
        error_resp.message = std::string("Requested block height: ") + std::to_string(req.height) + " greater than current top block height: " +  std::to_string(m_core.get_current_blockchain_height() - 1);
        return false;
      }
      block_hash = m_core.get_block_id_by_height(req.height);
    }
    block blk;
    bool orphan = false;
    bool have_block = m_core.get_block_by_hash(block_hash, blk, &orphan);
    if (!have_block)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: can't get block by hash. Hash = " + req.hash + '.';
      return false;
    }
    if (blk.miner_tx.vin.size() != 1 || blk.miner_tx.vin.front().type() != typeid(txin_gen))
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: coinbase transaction in the block has the wrong type";
      return false;
    }
    uint64_t block_height = boost::get<txin_gen>(blk.miner_tx.vin.front()).height;
    const bool restricted = m_restricted && ctx;
    bool response_filled = fill_block_header_response(blk, orphan, block_height, block_hash, res.block_header, req.fill_pow_hash && !restricted);
    if (!response_filled)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Internal error: can't produce valid response.";
      return false;
    }
    for (size_t n = 0; n < blk.tx_hashes.size(); ++n)
    {
      res.tx_hashes.push_back(epee::string_tools::pod_to_hex(blk.tx_hashes[n]));
    }
    res.blob = string_tools::buff_to_hex_nodelimer(t_serializable_object_to_blob(blk));
    res.json = obj_to_json_str(blk);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_connections(const COMMAND_RPC_GET_CONNECTIONS::request& req, COMMAND_RPC_GET_CONNECTIONS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_connections);

    res.connections = m_p2p.get_payload_object().get_connections();

    res.status = CORE_RPC_STATUS_OK;

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_info_json(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    return on_get_info(req, res, ctx);
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_hard_fork_info(const COMMAND_RPC_HARD_FORK_INFO::request& req, COMMAND_RPC_HARD_FORK_INFO::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_hard_fork_info);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_HARD_FORK_INFO>(invoke_http_mode::JON_RPC, "hard_fork_info", req, res, r))
      return r;

    const Blockchain &blockchain = m_core.get_blockchain_storage();
    uint8_t version = req.version > 0 ? req.version : blockchain.get_next_hard_fork_version();
    res.version = blockchain.get_current_hard_fork_version();
    res.enabled = blockchain.get_hard_fork_voting_info(version, res.window, res.votes, res.threshold, res.earliest_height, res.voting);
    res.state = blockchain.get_hard_fork_state();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_bans(const COMMAND_RPC_GETBANS::request& req, COMMAND_RPC_GETBANS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_bans);

    auto now = time(nullptr);
    std::map<std::string, time_t> blocked_hosts = m_p2p.get_blocked_hosts();
    for (std::map<std::string, time_t>::const_iterator i = blocked_hosts.begin(); i != blocked_hosts.end(); ++i)
    {
      if (i->second > now) {
        COMMAND_RPC_GETBANS::ban b;
        b.host = i->first;
        b.ip = 0;
        uint32_t ip;
        if (epee::string_tools::get_ip_int32_from_string(ip, b.host))
          b.ip = ip;
        b.seconds = i->second - now;
        res.bans.push_back(b);
      }
    }
    std::map<epee::net_utils::ipv4_network_subnet, time_t> blocked_subnets = m_p2p.get_blocked_subnets();
    for (std::map<epee::net_utils::ipv4_network_subnet, time_t>::const_iterator i = blocked_subnets.begin(); i != blocked_subnets.end(); ++i)
    {
      if (i->second > now) {
        COMMAND_RPC_GETBANS::ban b;
        b.host = i->first.host_str();
        b.ip = 0;
        b.seconds = i->second - now;
        res.bans.push_back(b);
      }
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_banned(const COMMAND_RPC_BANNED::request& req, COMMAND_RPC_BANNED::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_banned);

    auto na_parsed = net::get_network_address(req.address, 0);
    if (!na_parsed)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Unsupported host type";
      return false;
    }
    epee::net_utils::network_address na = std::move(*na_parsed);

    time_t seconds;
    if (m_p2p.is_host_blocked(na, &seconds))
    {
      res.banned = true;
      res.seconds = seconds;
    }
    else
    {
      res.banned = false;
      res.seconds = 0;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_set_bans(const COMMAND_RPC_SETBANS::request& req, COMMAND_RPC_SETBANS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_set_bans);

    for (auto i = req.bans.begin(); i != req.bans.end(); ++i)
    {
      epee::net_utils::network_address na;

      // try subnet first
      if (!i->host.empty())
      {
        auto ns_parsed = net::get_ipv4_subnet_address(i->host);
        if (ns_parsed)
        {
          if (i->ban)
            m_p2p.block_subnet(*ns_parsed, i->seconds);
          else
            m_p2p.unblock_subnet(*ns_parsed);
          continue;
        }
      }

      // then host
      if (!i->host.empty())
      {
        auto na_parsed = net::get_network_address(i->host, 0);
        if (!na_parsed)
        {
          error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
          error_resp.message = "Unsupported host/subnet type";
          return false;
        }
        na = std::move(*na_parsed);
      }
      else
      {
        na = epee::net_utils::ipv4_network_address{i->ip, 0};
      }
      if (i->ban)
        m_p2p.block_host(na, i->seconds);
      else
        m_p2p.unblock_host(na);
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_flush_txpool(const COMMAND_RPC_FLUSH_TRANSACTION_POOL::request& req, COMMAND_RPC_FLUSH_TRANSACTION_POOL::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_flush_txpool);

    bool failed = false;
    std::vector<crypto::hash> txids;
    if (req.txids.empty())
    {
      std::vector<transaction> pool_txs;
      m_core.get_pool().get_transactions(pool_txs);
      for (const auto &tx: pool_txs)
      {
        txids.push_back(cryptonote::get_transaction_hash(tx));
      }
    }
    else
    {
      for (const auto &str: req.txids)
      {
        cryptonote::blobdata txid_data;
        if(!epee::string_tools::parse_hexstr_to_binbuff(str, txid_data))
        {
          failed = true;
        }
        else
        {
          crypto::hash txid = *reinterpret_cast<const crypto::hash*>(txid_data.data());
          txids.push_back(txid);
        }
      }
    }
    if (!m_core.get_blockchain_storage().flush_txes_from_pool(txids))
    {
      res.status = "Failed to remove one or more tx(es)";
      return false;
    }

    if (failed)
    {
      if (txids.empty())
        res.status = "Failed to parse txid";
      else
        res.status = "Failed to parse some of the txids";
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_output_histogram(const COMMAND_RPC_GET_OUTPUT_HISTOGRAM::request& req, COMMAND_RPC_GET_OUTPUT_HISTOGRAM::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_output_histogram);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_OUTPUT_HISTOGRAM>(invoke_http_mode::JON_RPC, "get_output_histogram", req, res, r))
      return r;

    const bool restricted = m_restricted && ctx;
    if (restricted && req.recent_cutoff > 0 && req.recent_cutoff < (uint64_t)time(NULL) - OUTPUT_HISTOGRAM_RECENT_CUTOFF_RESTRICTION)
    {
      res.status = "Recent cutoff is too old";
      return true;
    }

    std::map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t>> histogram;
    try
    {
      histogram = m_core.get_blockchain_storage().get_output_histogram(req.amounts, req.unlocked, req.recent_cutoff, req.min_count);
    }
    catch (const std::exception &e)
    {
      res.status = "Failed to get output histogram";
      return true;
    }

    res.histogram.clear();
    res.histogram.reserve(histogram.size());
    for (const auto &i: histogram)
    {
      if (std::get<0>(i.second) >= req.min_count && (std::get<0>(i.second) <= req.max_count || req.max_count == 0))
        res.histogram.push_back(COMMAND_RPC_GET_OUTPUT_HISTOGRAM::entry(i.first, std::get<0>(i.second), std::get<1>(i.second), std::get<2>(i.second)));
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_version(const COMMAND_RPC_GET_VERSION::request& req, COMMAND_RPC_GET_VERSION::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_version);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_VERSION>(invoke_http_mode::JON_RPC, "get_version", req, res, r))
      return r;

    res.version = CORE_RPC_VERSION;
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_service_node_status(const COMMAND_RPC_GET_SERVICE_NODE_STATUS::request& req, COMMAND_RPC_GET_SERVICE_NODE_STATUS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_service_node_status);
    cryptonote::COMMAND_RPC_GET_SERVICE_NODE_KEY::response get_service_node_key_res = {};
    cryptonote::COMMAND_RPC_GET_SERVICE_NODE_KEY::request get_service_node_key_req = {};

    if (!on_get_service_node_key(get_service_node_key_req, get_service_node_key_res, error_resp, ctx))
    {
      return false;
    }

    cryptonote::COMMAND_RPC_GET_SERVICE_NODES::request get_service_nodes_req;
    cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response get_service_nodes_res;

    get_service_nodes_req.include_json = req.include_json;
    get_service_nodes_req.service_node_pubkeys.push_back(get_service_node_key_res.service_node_pubkey);

    if (!on_get_service_nodes(get_service_nodes_req, get_service_nodes_res, error_resp, ctx))
    {
      return false;
    }

    if (get_service_nodes_res.service_node_states.empty()) // Started in service node but not staked, no information on the blockchain yet
    {
      res.service_node_state.service_node_pubkey  = std::move(get_service_node_key_res.service_node_pubkey);
      res.service_node_state.version_major        = LOKI_VERSION[0];
      res.service_node_state.version_minor        = LOKI_VERSION[1];
      res.service_node_state.version_patch        = LOKI_VERSION[2];
      res.service_node_state.public_ip            = epee::string_tools::get_ip_string_from_int32(m_core.sn_public_ip());
      res.service_node_state.storage_port         = m_core.storage_port();
      res.service_node_state.storage_lmq_port     = m_core.m_storage_lmq_port;
      res.service_node_state.quorumnet_port       = m_core.quorumnet_port();
      res.service_node_state.pubkey_ed25519       = std::move(get_service_node_key_res.service_node_ed25519_pubkey);
      res.service_node_state.pubkey_x25519        = std::move(get_service_node_key_res.service_node_x25519_pubkey);
      res.service_node_state.service_node_version = LOKI_VERSION;
    }
    else
    {
      res.service_node_state = std::move(get_service_nodes_res.service_node_states[0]);
    }

    res.height = get_service_nodes_res.height;
    res.block_hash = get_service_nodes_res.block_hash;
    res.status = get_service_nodes_res.status;
    res.as_json = get_service_nodes_res.as_json;

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_coinbase_tx_sum(const COMMAND_RPC_GET_COINBASE_TX_SUM::request& req, COMMAND_RPC_GET_COINBASE_TX_SUM::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_coinbase_tx_sum);
    std::tie(res.emission_amount, res.fee_amount, res.burn_amount) = m_core.get_coinbase_tx_sum(req.height, req.count);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_base_fee_estimate(const COMMAND_RPC_GET_BASE_FEE_ESTIMATE::request& req, COMMAND_RPC_GET_BASE_FEE_ESTIMATE::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_base_fee_estimate);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_BASE_FEE_ESTIMATE>(invoke_http_mode::JON_RPC, "get_fee_estimate", req, res, r))
      return r;

    auto fees = m_core.get_blockchain_storage().get_dynamic_base_fee_estimate(req.grace_blocks);
    res.fee_per_byte = fees.first;
    res.fee_per_output = fees.second;
    res.quantization_mask = Blockchain::get_fee_quantization_mask();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_alternate_chains(const COMMAND_RPC_GET_ALTERNATE_CHAINS::request& req, COMMAND_RPC_GET_ALTERNATE_CHAINS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_alternate_chains);
    try
    {
      std::list<std::pair<Blockchain::block_extended_info, std::vector<crypto::hash>>> chains = m_core.get_blockchain_storage().get_alternative_chains();
      for (const auto &i: chains)
      {
        res.chains.push_back(COMMAND_RPC_GET_ALTERNATE_CHAINS::chain_info{epee::string_tools::pod_to_hex(get_block_hash(i.first.bl)), i.first.height, i.second.size(), i.first.cumulative_difficulty, {}, std::string()});
        res.chains.back().block_hashes.reserve(i.second.size());
        for (const crypto::hash &block_id: i.second)
          res.chains.back().block_hashes.push_back(epee::string_tools::pod_to_hex(block_id));
        if (i.first.height < i.second.size())
        {
          res.status = "Error finding alternate chain attachment point";
          return true;
        }
        cryptonote::block main_chain_parent_block;
        try { main_chain_parent_block = m_core.get_blockchain_storage().get_db().get_block_from_height(i.first.height - i.second.size()); }
        catch (const std::exception &e) { res.status = "Error finding alternate chain attachment point"; return true; }
        res.chains.back().main_chain_parent_block = epee::string_tools::pod_to_hex(get_block_hash(main_chain_parent_block));
      }
      res.status = CORE_RPC_STATUS_OK;
    }
    catch (...)
    {
      res.status = "Error retrieving alternate chains";
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_limit(const COMMAND_RPC_GET_LIMIT::request& req, COMMAND_RPC_GET_LIMIT::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_limit);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_LIMIT>(invoke_http_mode::JON, "/get_limit", req, res, r))
      return r;

    res.limit_down = epee::net_utils::connection_basic::get_rate_down_limit();
    res.limit_up = epee::net_utils::connection_basic::get_rate_up_limit();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_set_limit(const COMMAND_RPC_SET_LIMIT::request& req, COMMAND_RPC_SET_LIMIT::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_set_limit);
    // -1 = reset to default
    //  0 = do not modify

    if (req.limit_down > 0)
    {
      epee::net_utils::connection_basic::set_rate_down_limit(req.limit_down);
    }
    else if (req.limit_down < 0)
    {
      if (req.limit_down != -1)
      {
        res.status = CORE_RPC_ERROR_CODE_WRONG_PARAM;
        return false;
      }
      epee::net_utils::connection_basic::set_rate_down_limit(nodetool::default_limit_down);
    }

    if (req.limit_up > 0)
    {
      epee::net_utils::connection_basic::set_rate_up_limit(req.limit_up);
    }
    else if (req.limit_up < 0)
    {
      if (req.limit_up != -1)
      {
        res.status = CORE_RPC_ERROR_CODE_WRONG_PARAM;
        return false;
      }
      epee::net_utils::connection_basic::set_rate_up_limit(nodetool::default_limit_up);
    }

    res.limit_down = epee::net_utils::connection_basic::get_rate_down_limit();
    res.limit_up = epee::net_utils::connection_basic::get_rate_up_limit();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_out_peers(const COMMAND_RPC_OUT_PEERS::request& req, COMMAND_RPC_OUT_PEERS::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_out_peers);
    m_p2p.change_max_out_public_peers(req.out_peers);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_in_peers(const COMMAND_RPC_IN_PEERS::request& req, COMMAND_RPC_IN_PEERS::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_in_peers);
    m_p2p.change_max_in_public_peers(req.in_peers);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_update(const COMMAND_RPC_UPDATE::request& req, COMMAND_RPC_UPDATE::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_update);

    if (m_core.offline())
    {
      res.status = "Daemon is running offline";
      return true;
    }

    static const char software[] = "loki";
#ifdef BUILD_TAG
    static const char buildtag[] = BOOST_PP_STRINGIZE(BUILD_TAG);
    static const char subdir[] = "cli";
#else
    static const char buildtag[] = "source";
    static const char subdir[] = "source";
#endif

    if (req.command != "check" && req.command != "download" && req.command != "update")
    {
      res.status = std::string("unknown command: '") + req.command + "'";
      return true;
    }

    std::string version, hash;
    if (!tools::check_updates(software, buildtag, version, hash))
    {
      res.status = "Error checking for updates";
      return true;
    }
    if (tools::vercmp(version.c_str(), GRAFT_VERSION) <= 0)
    {
      res.update = false;
      res.status = CORE_RPC_STATUS_OK;
      return true;
    }
    res.update = true;
    res.version = version;
    res.user_uri = tools::get_update_url(software, subdir, buildtag, version, true);
    res.auto_uri = tools::get_update_url(software, subdir, buildtag, version, false);
    res.hash = hash;
    if (req.command == "check")
    {
      res.status = CORE_RPC_STATUS_OK;
      return true;
    }

    boost::filesystem::path path;
    if (req.path.empty())
    {
      std::string filename;
      const char *slash = strrchr(res.auto_uri.c_str(), '/');
      if (slash)
        filename = slash + 1;
      else
        filename = std::string(software) + "-update-" + version;
      path = epee::string_tools::get_current_module_folder();
      path /= filename;
    }
    else
    {
      path = req.path;
    }

    crypto::hash file_hash;
    if (!tools::sha256sum(path.string(), file_hash) || (hash != epee::string_tools::pod_to_hex(file_hash)))
    {
      MDEBUG("We don't have that file already, downloading");
      if (!tools::download(path.string(), res.auto_uri))
      {
        MERROR("Failed to download " << res.auto_uri);
        return false;
      }
      if (!tools::sha256sum(path.string(), file_hash))
      {
        MERROR("Failed to hash " << path);
        return false;
      }
      if (hash != epee::string_tools::pod_to_hex(file_hash))
      {
        MERROR("Download from " << res.auto_uri << " does not match the expected hash");
        return false;
      }
      MINFO("New version downloaded to " << path);
    }
    else
    {
      MDEBUG("We already have " << path << " with expected hash");
    }
    res.path = path.string();

    if (req.command == "download")
    {
      res.status = CORE_RPC_STATUS_OK;
      return true;
    }

    res.status = "'update' not implemented yet";
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_pop_blocks(const COMMAND_RPC_POP_BLOCKS::request& req, COMMAND_RPC_POP_BLOCKS::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_pop_blocks);

    m_core.get_blockchain_storage().pop_blocks(req.nblocks);

    res.height = m_core.get_current_blockchain_height();
    res.status = CORE_RPC_STATUS_OK;

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_relay_tx(const COMMAND_RPC_RELAY_TX::request& req, COMMAND_RPC_RELAY_TX::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_relay_tx);

    bool failed = false;
    res.status = "";
    for (const auto &str: req.txids)
    {
      cryptonote::blobdata txid_data;
      if(!epee::string_tools::parse_hexstr_to_binbuff(str, txid_data))
      {
        if (!res.status.empty()) res.status += ", ";
        res.status += std::string("invalid transaction id: ") + str;
        failed = true;
        continue;
      }
      crypto::hash txid = *reinterpret_cast<const crypto::hash*>(txid_data.data());

      cryptonote::blobdata txblob;
      bool r = m_core.get_pool().get_transaction(txid, txblob);
      if (r)
      {
        cryptonote_connection_context fake_context{};
        NOTIFY_NEW_TRANSACTIONS::request r;
        r.txs.push_back(txblob);
        m_core.get_protocol()->relay_transactions(r, fake_context);
        //TODO: make sure that tx has reached other nodes here, probably wait to receive reflections from other nodes
      }
      else
      {
        if (!res.status.empty()) res.status += ", ";
        res.status += std::string("transaction not found in pool: ") + str;
        failed = true;
        continue;
      }
    }

    if (failed)
    {
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_sync_info(const COMMAND_RPC_SYNC_INFO::request& req, COMMAND_RPC_SYNC_INFO::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_sync_info);

    crypto::hash top_hash;
    m_core.get_blockchain_top(res.height, top_hash);
    ++res.height; // turn top block height into blockchain height
    res.target_height = m_core.get_target_blockchain_height();
    res.next_needed_pruning_seed = m_p2p.get_payload_object().get_next_needed_pruning_stripe().second;

    for (const auto &c: m_p2p.get_payload_object().get_connections())
      res.peers.push_back({c});
    const cryptonote::block_queue &block_queue = m_p2p.get_payload_object().get_block_queue();
    block_queue.foreach([&](const cryptonote::block_queue::span &span) {
      const std::string span_connection_id = epee::string_tools::pod_to_hex(span.connection_id);
      uint32_t speed = (uint32_t)(100.0f * block_queue.get_speed(span.connection_id) + 0.5f);
      std::string address = "";
      for (const auto &c: m_p2p.get_payload_object().get_connections())
        if (c.connection_id == span_connection_id)
          address = c.address;
      res.spans.push_back({span.start_block_height, span.nblocks, span_connection_id, (uint32_t)(span.rate + 0.5f), speed, span.size, address});
      return true;
    });
    res.overview = block_queue.get_overview(res.height);

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_txpool_backlog(const COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG::request& req, COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_txpool_backlog);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG>(invoke_http_mode::JON_RPC, "get_txpool_backlog", req, res, r))
      return r;

    m_core.get_pool().get_transaction_backlog(res.backlog);
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_output_distribution(const COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request& req, COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_output_distribution);
    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_OUTPUT_DISTRIBUTION>(invoke_http_mode::JON_RPC, "get_output_distribution", req, res, r))
      return r;

    try
    {
      // 0 is placeholder for the whole chain
      const uint64_t req_to_height = req.to_height ? req.to_height : (m_core.get_current_blockchain_height() - 1);
      for (uint64_t amount: req.amounts)
      {
        auto data = rpc::RpcHandler::get_output_distribution([this](uint64_t amount, uint64_t from, uint64_t to, uint64_t &start_height, std::vector<uint64_t> &distribution, uint64_t &base) { return m_core.get_output_distribution(amount, from, to, start_height, distribution, base); }, amount, req.from_height, req_to_height, [this](uint64_t height) { return m_core.get_blockchain_storage().get_db().get_block_hash_from_height(height); }, req.cumulative, m_core.get_current_blockchain_height());
        if (!data)
        {
          error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
          error_resp.message = "Failed to get output distribution";
          return false;
        }

        res.distributions.push_back({std::move(*data), amount, "", req.binary, req.compress});
      }
    }
    catch (const std::exception &e)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Failed to get output distribution";
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_output_distribution_bin(const COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::request& req, COMMAND_RPC_GET_OUTPUT_DISTRIBUTION::response& res, const connection_context *ctx)
  {
    PERF_TIMER(on_get_output_distribution_bin);

    bool r;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_OUTPUT_DISTRIBUTION>(invoke_http_mode::BIN, "/get_output_distribution.bin", req, res, r))
      return r;

    res.status = "Failed";

    res.status = "Failed";

    if (!req.binary)
    {
      res.status = "Binary only call";
      return false;
    }
    try
    {
      // 0 is placeholder for the whole chain
      const uint64_t req_to_height = req.to_height ? req.to_height : (m_core.get_current_blockchain_height() - 1);
      for (uint64_t amount: req.amounts)
      {
        auto data = rpc::RpcHandler::get_output_distribution([this](uint64_t amount, uint64_t from, uint64_t to, uint64_t &start_height, std::vector<uint64_t> &distribution, uint64_t &base) { return m_core.get_output_distribution(amount, from, to, start_height, distribution, base); }, amount, req.from_height, req_to_height, [this](uint64_t height) { return m_core.get_blockchain_storage().get_db().get_block_hash_from_height(height); }, req.cumulative, m_core.get_current_blockchain_height());
        if (!data)
        {
          res.status = "Failed to get output distribution";
          return false;
        }

        res.distributions.push_back({std::move(*data), amount, "", req.binary, req.compress});
      }
    }
    catch (const std::exception &e)
    {
      res.status = "Failed to get output distribution";
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_prune_blockchain(const COMMAND_RPC_PRUNE_BLOCKCHAIN::request& req, COMMAND_RPC_PRUNE_BLOCKCHAIN::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    try
    {
      if (!(req.check ? m_core.check_blockchain_pruning() : m_core.prune_blockchain()))
      {
        error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
        error_resp.message = req.check ? "Failed to check blockchain pruning" : "Failed to prune blockchain";
        return false;
      }
      res.pruning_seed = m_core.get_blockchain_pruning_seed();
      res.pruned = res.pruning_seed != 0;
    }
    catch (const std::exception &e)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Failed to prune blockchain";
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_supernode_announce(const COMMAND_RPC_SUPERNODE_ANNOUNCE::request &req, COMMAND_RPC_SUPERNODE_ANNOUNCE::response &res, json_rpc::error &error_resp)
  {
      LOG_PRINT_L0("RPC Request: on_supernode_announce: start");
      // send p2p announce
      m_p2p.add_supernode(req.supernode_public_id, req.network_address);
      m_p2p.do_supernode_announce(req);
      res.status = 0;
      LOG_PRINT_L0("RPC Request: on_supernode_announce: end");
      return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_supernode_stakes(const COMMAND_RPC_SUPERNODE_GET_STAKES::request &req, COMMAND_RPC_SUPERNODE_GET_STAKES::response &res, json_rpc::error &error_resp)
  {
      LOG_PRINT_L0("RPC Request: on_supernode_stakes: start");
      // send p2p stakes
      m_p2p.add_supernode(req.supernode_public_id, req.network_address);
      m_p2p.send_stakes_to_supernode();
      res.status = 0;
      LOG_PRINT_L0("RPC Request: on_supernode_stakes: end");
      return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_supernode_blockchain_based_list(const COMMAND_RPC_SUPERNODE_GET_BLOCKCHAIN_BASED_LIST::request &req, COMMAND_RPC_SUPERNODE_GET_BLOCKCHAIN_BASED_LIST::response &res, json_rpc::error &error_resp)
  {
      LOG_PRINT_L0("RPC Request: on_supernode_blockchain_based_list: start");
      // send p2p stake txs
      m_p2p.add_supernode(req.supernode_public_id, req.network_address);
      m_p2p.send_blockchain_based_list_to_supernode(req.last_received_block_height);
      res.status = 0;
      LOG_PRINT_L0("RPC Request: on_supernode_blockchain_based_list: end");
      return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_broadcast(const COMMAND_RPC_BROADCAST::request &req, COMMAND_RPC_BROADCAST::response &res, json_rpc::error &error_resp)
  {
      LOG_PRINT_L0("RPC Request: on_broadcast: start");
      cryptonote::account_public_address acc = AUTO_VAL_INIT(acc);
      std::string sender_address = req.sender_address;
      crypto::public_key dummy_key;
      if (!sender_address.empty() && !epee::string_tools::hex_to_pod(sender_address, dummy_key))
      {
          error_resp.code = CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS;
          error_resp.message = "Failed to parse sender id";
          return false;
      }

      m_p2p.do_broadcast(req);
      res.status = 0;
      LOG_PRINT_L0("RPC Request: on_broadcast: end");
      return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_multicast(const COMMAND_RPC_MULTICAST::request &req, COMMAND_RPC_MULTICAST::response &res, json_rpc::error &error_resp)
  {
      LOG_PRINT_L0("RPC Request: on_multicast: start");
      cryptonote::account_public_address acc = AUTO_VAL_INIT(acc);
      crypto::public_key dummy_key;
      for (auto addr : req.receiver_addresses)
      {
          if (addr.empty() || !epee::string_tools::hex_to_pod(addr, dummy_key))
          {
              error_resp.code = CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS;
              error_resp.message = "Failed to parse receiver id";
              return false;
          }
      }

      std::string sender_address = req.sender_address;
      if (!sender_address.empty() && !epee::string_tools::hex_to_pod(sender_address, dummy_key))
      {
          error_resp.code = CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS;
          error_resp.message = "Failed to parse sender id";
          return false;
      }

      m_p2p.do_multicast(req);
      res.status = 0;
      LOG_PRINT_L0("RPC Request: on_multicast: end");
      return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_unicast(const COMMAND_RPC_UNICAST::request &req, COMMAND_RPC_UNICAST::response &res, json_rpc::error &error_resp)
  {
      LOG_PRINT_L0("RPC Request: on_unicast: start");
      cryptonote::account_public_address acc = AUTO_VAL_INIT(acc);

      std::string receiver_address = req.receiver_address;
      crypto::public_key dummy_key;
      if (receiver_address.empty() || !epee::string_tools::hex_to_pod(receiver_address, dummy_key))
      {
          error_resp.code = CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS;
          error_resp.message = "Failed to parse receiver id";
          return false;
      }

      std::string sender_address = req.sender_address;
      if (!sender_address.empty() && !epee::string_tools::hex_to_pod(sender_address, dummy_key))
      {
          error_resp.code = CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS;
          error_resp.message = "Failed to parse sender id";
          return false;
      }

      m_p2p.do_unicast(req);
      res.status = 0;
      LOG_PRINT_L0("RPC Request: on_unicast: end");
      return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_tunnels(const COMMAND_RPC_TUNNEL_DATA::request &req, COMMAND_RPC_TUNNEL_DATA::response &res, json_rpc::error &error_resp)
  {
      LOG_PRINT_L0("RPC Request: on_get_tunnels: start");
      res.supernodes_addresses = m_p2p.get_supernodes_addresses();
      res.tunnels = m_p2p.get_tunnels();
      // Temporary backwards compatibility (remove me along with the supernode_address member): store the first SN
      if (res.supernodes_addresses.size() > 0)
          res.supernode_address = res.supernodes_addresses[0];
      LOG_PRINT_L0("RPC Request: on_get_tunnels: end");
      return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------

  bool core_rpc_server::on_get_rta_stats(const COMMAND_RPC_RTA_STATS::request &req, COMMAND_RPC_RTA_STATS::response &res, epee::json_rpc::error &error_resp)
  {
      res.announce_bytes_in = m_p2p.get_announce_bytes_in();
      res.announce_bytes_out = m_p2p.get_announce_bytes_out();
      res.broadcast_bytes_in = m_p2p.get_broadcast_bytes_in();
      res.broadcast_bytes_out = m_p2p.get_broadcast_bytes_out();
      res.multicast_bytes_in = m_p2p.get_multicast_bytes_in();
      res.multicast_bytes_out = m_p2p.get_multicast_bytes_out();
      return true;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_prune_blockchain(const COMMAND_RPC_PRUNE_BLOCKCHAIN::request& req, COMMAND_RPC_PRUNE_BLOCKCHAIN::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    try
    {
      if (!(req.check ? m_core.check_blockchain_pruning() : m_core.prune_blockchain()))
      {
        error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
        error_resp.message = req.check ? "Failed to check blockchain pruning" : "Failed to prune blockchain";
        return false;
      }
      res.pruning_seed = m_core.get_blockchain_pruning_seed();
      res.pruned = res.pruning_seed != 0;
    }
    catch (const std::exception &e)
    {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Failed to prune blockchain";
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }

  const command_line::arg_descriptor<std::string, false, true, 2> core_rpc_server::arg_rpc_bind_port = {
      "rpc-bind-port"
    , "Port for RPC server"
    , std::to_string(config::RPC_DEFAULT_PORT)
    , {{ &cryptonote::arg_testnet_on, &cryptonote::arg_stagenet_on }}
    , [](std::array<bool, 2> testnet_stagenet, bool defaulted, std::string val)->std::string {
        if (testnet_stagenet[0] && defaulted)
          return std::to_string(config::testnet::RPC_DEFAULT_PORT);
        else if (testnet_stagenet[1] && defaulted)
          return std::to_string(config::stagenet::RPC_DEFAULT_PORT);
        return val;
      }
    };

  const command_line::arg_descriptor<std::string> core_rpc_server::arg_bootstrap_daemon_login = {
      "bootstrap-daemon-login"
    , "Specify username:password for the bootstrap daemon login"
    , ""
    };

  const command_line::arg_descriptor<std::string> core_rpc_server::arg_rpc_restricted_bind_port = {
      "rpc-restricted-bind-port"
    , "Port for restricted RPC server"
    , ""
    };

  const command_line::arg_descriptor<bool> core_rpc_server::arg_restricted_rpc = {
      "restricted-rpc"
    , "Restrict RPC to view only commands and do not return privacy sensitive data in RPC calls"
    , false
    };

  const command_line::arg_descriptor<std::string> core_rpc_server::arg_bootstrap_daemon_address = {
      "bootstrap-daemon-address"
    , "URL of a 'bootstrap' remote daemon that the connected wallets can use while this daemon is still not fully synced"
    , ""
    };

  //
  // Loki
  //
  const command_line::arg_descriptor<int> core_rpc_server::arg_rpc_long_poll_connections = {
      "rpc-long-poll-connections"
    , "Number of RPC connections allocated for long polling wallet queries to the TX pool"
    , 16
    };

  bool core_rpc_server::on_get_quorum_state(const COMMAND_RPC_GET_QUORUM_STATE::request& req, COMMAND_RPC_GET_QUORUM_STATE::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_quorum_state);

    if (req.quorum_type >= tools::enum_count<service_nodes::quorum_type> &&
        req.quorum_type != COMMAND_RPC_GET_QUORUM_STATE::ALL_QUORUMS_SENTINEL_VALUE)
    {
      error_resp.code    = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Quorum type specifies an invalid value: ";
      error_resp.message += std::to_string(req.quorum_type);
      res.status         = error_resp.message;
      return false;
    }

    uint64_t start = req.start_height, end = req.end_height;
    if (start == COMMAND_RPC_GET_QUORUM_STATE::HEIGHT_SENTINEL_VALUE &&
        end == COMMAND_RPC_GET_QUORUM_STATE::HEIGHT_SENTINEL_VALUE)
    {
      start = m_core.get_blockchain_storage().get_current_blockchain_height() - 1;
      end   = start + 1;
    }
    else if (start == COMMAND_RPC_GET_QUORUM_STATE::HEIGHT_SENTINEL_VALUE)
    {
      start = end;
      end   = end + 1;
    }
    else if (end == COMMAND_RPC_GET_QUORUM_STATE::HEIGHT_SENTINEL_VALUE)
    {
      end = start + 1;
    }
    else
    {
      if (end > start) end++;
      else
      {
        if (end != 0)
          end--;
      }
    }

    uint64_t curr_height = m_core.get_blockchain_storage().get_current_blockchain_height();
    start                = std::min(curr_height, start);
    end                  = std::min(curr_height, end);

    uint64_t count       = (start > end) ? start - end : end - start;
    if (ctx && m_restricted)
    {
      if (count > COMMAND_RPC_GET_QUORUM_STATE_MAX_COUNT)
      {
        error_resp.code     = CORE_RPC_ERROR_CODE_WRONG_PARAM;
        error_resp.message  = "Number of requested quorums greater than the allowed limit: ";
        error_resp.message += std::to_string(COMMAND_RPC_GET_QUORUM_STATE_MAX_COUNT);
        error_resp.message += ", requested: ";
        error_resp.message += std::to_string(count);
        return false;
      }
    }

    bool at_least_one_succeeded = false;
    res.quorums.reserve(std::min((uint64_t)16, count));
    for (size_t height = start; height != end;)
    {
      uint8_t hf_version = m_core.get_hard_fork_version(height);
      if (hf_version != HardFork::INVALID_HF_VERSION_FOR_HEIGHT)
      {
        auto start_quorum_iterator = static_cast<service_nodes::quorum_type>(0);
        auto end_quorum_iterator   = service_nodes::max_quorum_type_for_hf(hf_version);

        if (req.quorum_type != COMMAND_RPC_GET_QUORUM_STATE::ALL_QUORUMS_SENTINEL_VALUE)
        {
          start_quorum_iterator = static_cast<service_nodes::quorum_type>(req.quorum_type);
          end_quorum_iterator   = start_quorum_iterator;
        }

        for (int quorum_int = (int)start_quorum_iterator; quorum_int <= (int)end_quorum_iterator; quorum_int++)
        {
          auto type = static_cast<service_nodes::quorum_type>(quorum_int);
          if (std::shared_ptr<const service_nodes::quorum> quorum = m_core.get_quorum(type, height, true /*include_old*/))
          {
            COMMAND_RPC_GET_QUORUM_STATE::quorum_for_height entry = {};
            entry.height                                          = height;
            entry.quorum_type                                     = static_cast<uint8_t>(quorum_int);

            entry.quorum.validators.reserve(quorum->validators.size());
            entry.quorum.workers.reserve(quorum->workers.size());
            for (crypto::public_key const &key : quorum->validators) entry.quorum.validators.push_back(epee::string_tools::pod_to_hex(key));
            for (crypto::public_key const &key : quorum->workers)    entry.quorum.workers.push_back(epee::string_tools::pod_to_hex(key));

            res.quorums.push_back(entry);
            at_least_one_succeeded = true;
          }
        }
      }

      if (end >= start) height++;
      else height--;
    }

    if (at_least_one_succeeded)
    {
      res.status = CORE_RPC_STATUS_OK;
    }
    else
    {
      error_resp.code    = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Failed to query any quorums at all";
    }

    return at_least_one_succeeded;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_service_node_registration_cmd_raw(const COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW::request& req,
                                                                 COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW::response& res,
                                                                 epee::json_rpc::error& error_resp,
                                                                 const connection_context *ctx)
  {
    PERF_TIMER(on_get_service_node_registration_cmd_raw);

    auto keys = m_core.get_service_node_keys();
    if (!keys)
    {
      error_resp.code    = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Daemon has not been started in service node mode, please relaunch with --service-node flag.";
      return false;
    }

    std::string err_msg;
    uint8_t hf_version = m_core.get_hard_fork_version(m_core.get_current_blockchain_height());
    if (!service_nodes::make_registration_cmd(m_core.get_nettype(), hf_version, req.staking_requirement, req.args, *keys, res.registration_cmd, req.make_friendly, err_msg))
    {
      error_resp.code    = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Failed to make registration command";
      if (err_msg != "")
        error_resp.message += ": " + err_msg;
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_service_node_registration_cmd(const COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD::request& req,
                                                             COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD::response& res,
                                                             epee::json_rpc::error& error_resp,
                                                             const connection_context *ctx)
  {
    PERF_TIMER(on_get_service_node_registration_cmd);

    std::vector<std::string> args;

    uint64_t const curr_height   = m_core.get_current_blockchain_height();
    uint64_t staking_requirement = service_nodes::get_staking_requirement(m_core.get_nettype(), curr_height, m_core.get_hard_fork_version(curr_height));

    {
      uint64_t portions_cut;
      if (!service_nodes::get_portions_from_percent_str(req.operator_cut, portions_cut))
      {
        MERROR("Invalid value: " << req.operator_cut << ". Should be between [0-100]");
        return false;
      }

      args.push_back(std::to_string(portions_cut));
    }

    for (const auto contrib : req.contributions)
    {
        uint64_t num_portions = service_nodes::get_portions_to_make_amount(staking_requirement, contrib.amount);
        args.push_back(contrib.address);
        args.push_back(std::to_string(num_portions));
    }

    COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW::request req_old;
    COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW::response res_old;

    req_old.staking_requirement = req.staking_requirement;
    req_old.args = std::move(args);
    req_old.make_friendly = false;

    const bool success = on_get_service_node_registration_cmd_raw(req_old, res_old, error_resp);

    res.status = res_old.status;
    res.registration_cmd = res_old.registration_cmd;

    return success;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_service_node_blacklisted_key_images(const COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES::request& req, COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES::response& res, epee::json_rpc::error &error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_service_node_blacklisted_key_images);
    auto &blacklist = m_core.get_service_node_blacklisted_key_images();

    res.status = CORE_RPC_STATUS_OK;
    res.blacklist.reserve(blacklist.size());
    for (const service_nodes::key_image_blacklist_entry &entry : blacklist)
    {
      res.blacklist.emplace_back();
      auto &new_entry = res.blacklist.back();
      new_entry.key_image     = epee::string_tools::pod_to_hex(entry.key_image);
      new_entry.unlock_height = entry.unlock_height;
      new_entry.amount = entry.amount;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_service_node_key(const COMMAND_RPC_GET_SERVICE_NODE_KEY::request& req, COMMAND_RPC_GET_SERVICE_NODE_KEY::response& res, epee::json_rpc::error &error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_service_node_key);

    if (auto keys = m_core.get_service_node_keys())
    {
      res.service_node_pubkey = string_tools::pod_to_hex(keys->pub);
      res.service_node_ed25519_pubkey = string_tools::pod_to_hex(keys->pub_ed25519);
      res.service_node_x25519_pubkey = string_tools::pod_to_hex(keys->pub_x25519);
      res.status = CORE_RPC_STATUS_OK;
      return true;
    }

    error_resp.code    = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
    error_resp.message = "Daemon queried is not a service node or did not launch with --service-node";
    return false;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_service_node_privkey(const COMMAND_RPC_GET_SERVICE_NODE_PRIVKEY::request& req, COMMAND_RPC_GET_SERVICE_NODE_PRIVKEY::response& res, epee::json_rpc::error &error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_service_node_key);

    if (auto keys = m_core.get_service_node_keys())
    {
      res.service_node_privkey = string_tools::pod_to_hex(keys->key.data);
      res.service_node_ed25519_privkey = string_tools::pod_to_hex(keys->key_ed25519.data);
      res.service_node_x25519_privkey = string_tools::pod_to_hex(keys->key_x25519.data);
      res.status = CORE_RPC_STATUS_OK;
      return true;
    }

    error_resp.code    = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
    error_resp.message = "Daemon queried is not a service node or did not launch with --service-node";
    return false;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  template<typename response>
  void core_rpc_server::fill_sn_response_entry(response &entry, const service_nodes::service_node_pubkey_info &sn_info, uint64_t current_height) {

    const auto &info = *sn_info.info;
    entry.service_node_pubkey           = string_tools::pod_to_hex(sn_info.pubkey);
    entry.registration_height           = info.registration_height;
    entry.requested_unlock_height       = info.requested_unlock_height;
    entry.last_reward_block_height      = info.last_reward_block_height;
    entry.last_reward_transaction_index = info.last_reward_transaction_index;
    entry.active                        = info.is_active();
    entry.funded                        = info.is_fully_funded();
    entry.state_height                  = info.is_fully_funded()
        ? (info.is_decommissioned() ? info.last_decommission_height : info.active_since_height) : info.last_reward_block_height;
    entry.earned_downtime_blocks        = service_nodes::quorum_cop::calculate_decommission_credit(info, current_height);
    entry.decommission_count            = info.decommission_count;

    m_core.get_service_node_list().access_proof(sn_info.pubkey, [&entry](const auto &proof) {
        entry.service_node_version     = proof.version;
        entry.public_ip                = string_tools::get_ip_string_from_int32(proof.public_ip);
        entry.storage_port             = proof.storage_port;
        entry.storage_lmq_port         = proof.storage_lmq_port;
        entry.storage_server_reachable = proof.storage_server_reachable;
        entry.pubkey_ed25519           = proof.pubkey_ed25519 ? string_tools::pod_to_hex(proof.pubkey_ed25519) : "";
        entry.pubkey_x25519            = proof.pubkey_x25519 ? string_tools::pod_to_hex(proof.pubkey_x25519) : "";
        entry.quorumnet_port           = proof.quorumnet_port;

        // NOTE: Service Node Testing
        entry.last_uptime_proof                  = proof.timestamp;
        entry.storage_server_reachable           = proof.storage_server_reachable;
        entry.storage_server_reachable_timestamp = proof.storage_server_reachable_timestamp;
        entry.version_major                      = proof.version[0];
        entry.version_minor                      = proof.version[1];
        entry.version_patch                      = proof.version[2];
        entry.votes = std::vector<service_nodes::checkpoint_vote_record>(proof.votes.begin(), proof.votes.end());
    });

    entry.contributors.reserve(info.contributors.size());

    using namespace service_nodes;
    for (service_node_info::contributor_t const &contributor : info.contributors)
    {
      entry.contributors.push_back({});
      auto &new_contributor = entry.contributors.back();
      new_contributor.amount   = contributor.amount;
      new_contributor.reserved = contributor.reserved;
      new_contributor.address  = cryptonote::get_account_address_as_str(m_core.get_nettype(), false/*is_subaddress*/, contributor.address);

      new_contributor.locked_contributions.reserve(contributor.locked_contributions.size());
      for (service_node_info::contribution_t const &src : contributor.locked_contributions)
      {
        new_contributor.locked_contributions.push_back({});
        auto &dest = new_contributor.locked_contributions.back();
        dest.amount                                                = src.amount;
        dest.key_image                                             = string_tools::pod_to_hex(src.key_image);
        dest.key_image_pub_key                                     = string_tools::pod_to_hex(src.key_image_pub_key);
      }
    }

    entry.total_contributed             = info.total_contributed;
    entry.total_reserved                = info.total_reserved;
    entry.staking_requirement           = info.staking_requirement;
    entry.portions_for_operator         = info.portions_for_operator;
    entry.operator_address              = cryptonote::get_account_address_as_str(m_core.get_nettype(), false/*is_subaddress*/, info.operator_address);
    entry.swarm_id                      = info.swarm_id;
    entry.registration_hf_version       = info.registration_hf_version;

  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_service_nodes(const COMMAND_RPC_GET_SERVICE_NODES::request& req, COMMAND_RPC_GET_SERVICE_NODES::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_service_nodes);

    std::vector<crypto::public_key> pubkeys(req.service_node_pubkeys.size());
    for (size_t i = 0; i < req.service_node_pubkeys.size(); i++)
    {
      if (!string_tools::hex_to_pod(req.service_node_pubkeys[i], pubkeys[i]))
      {
        error_resp.code    = CORE_RPC_ERROR_CODE_WRONG_PARAM;
        error_resp.message = "Could not convert to a public key, arg: ";
        error_resp.message += std::to_string(i);
        error_resp.message += " which is pubkey: ";
        error_resp.message += req.service_node_pubkeys[i];
        return false;
      }
    }

    auto pubkey_info_list = m_core.get_service_node_list_state(pubkeys);

    res.status = CORE_RPC_STATUS_OK;
    res.service_node_states.reserve(pubkey_info_list.size());

    if (req.include_json)
    {
      res.as_json = "{\n}";
      if (pubkey_info_list.size() > 0) {
        res.as_json = cryptonote::obj_to_json_str(pubkey_info_list);
      }
    }

    const uint64_t height = m_core.get_current_blockchain_height();

    res.height = height - 1;
    res.block_hash = string_tools::pod_to_hex(m_core.get_block_id_by_height(res.height));
    
    for (auto &pubkey_info : pubkey_info_list)
    {
      COMMAND_RPC_GET_SERVICE_NODES::response::entry entry = {};
      fill_sn_response_entry(entry, pubkey_info, height);
      res.service_node_states.push_back(entry);
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_n_service_nodes(const COMMAND_RPC_GET_N_SERVICE_NODES::request& req,
                                               COMMAND_RPC_GET_N_SERVICE_NODES::response& res,
                                               epee::json_rpc::error&,
                                               const connection_context*)
  {
    res.status = CORE_RPC_STATUS_OK;
    const uint64_t height = m_core.get_current_blockchain_height();
    res.height = height - 1;
    res.target_height = m_core.get_target_blockchain_height();
    res.block_hash = string_tools::pod_to_hex(m_core.get_block_id_by_height(res.height));
    res.hardfork = m_core.get_hard_fork_version(res.height);

    if (!req.poll_block_hash.empty()) {
      res.polling_mode = true;
      if (req.poll_block_hash == res.block_hash) {
        res.unchanged = true;
        res.fields = req.fields;
        return true;
      }
    }

    std::vector<service_nodes::service_node_pubkey_info> sn_infos = m_core.get_service_node_list_state();

    if (req.active_only) {
      const auto end =
        std::remove_if(sn_infos.begin(), sn_infos.end(), [](const service_nodes::service_node_pubkey_info& snpk_info) {
          return !snpk_info.info->is_active();
        });
      
      sn_infos.erase(end, sn_infos.end());
    }

    if (req.limit != 0) {

      const auto limit = std::min(sn_infos.size(), static_cast<size_t>(req.limit));

      std::shuffle(sn_infos.begin(), sn_infos.end(), tools::rng);

      sn_infos.resize(limit);
    }

    res.service_node_states.reserve(sn_infos.size());


    for (auto &pubkey_info : sn_infos) {
      COMMAND_RPC_GET_N_SERVICE_NODES::response::entry entry = {res.fields};

      fill_sn_response_entry(entry, pubkey_info, height);

      res.service_node_states.push_back(entry);
    }

    res.fields = req.fields;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_all_service_nodes(const COMMAND_RPC_GET_SERVICE_NODES::request& req, COMMAND_RPC_GET_SERVICE_NODES::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    auto req_all = req;
    req_all.service_node_pubkeys.clear();
    return on_get_service_nodes(req_all, res, error_resp);
  }
  //------------------------------------------------------------------------------------------------------------------------------
  /// Start with seed and perform a series of computation arriving at the answer
  static uint64_t perform_blockchain_test_routine(const cryptonote::core& core,
                                                  uint64_t max_height,
                                                  uint64_t seed)
  {
    /// Should be sufficiently large to make it impractical
    /// to query remote nodes
    constexpr size_t NUM_ITERATIONS = 1000;

    std::mt19937_64 mt(seed);

    crypto::hash hash;

    uint64_t height = seed;

    for (auto i = 0u; i < NUM_ITERATIONS; ++i)
    {
      height = height % (max_height + 1);

      hash = core.get_block_id_by_height(height);

      using blob_t = cryptonote::blobdata;
      using block_pair_t = std::pair<blob_t, block>;

      /// pick a random byte from the block blob
      std::vector<block_pair_t> blocks;
      std::vector<blob_t> txs;
      if (!core.get_blockchain_storage().get_blocks(height, 1, blocks, txs)) {
        MERROR("Could not query block at requested height: " << height);
        return 0;
      }
      const blob_t &blob = blocks.at(0).first;
      const uint64_t byte_idx = tools::uniform_distribution_portable(mt, blob.size());
      uint8_t byte = blob[byte_idx];

      /// pick a random byte from a random transaction blob if found
      if (!txs.empty()) {
        const uint64_t tx_idx = tools::uniform_distribution_portable(mt, txs.size());
        const blob_t &tx_blob = txs[tx_idx];

        /// not sure if this can be empty, so check to be safe
        if (!tx_blob.empty()) {
          const uint64_t byte_idx = tools::uniform_distribution_portable(mt, tx_blob.size());
          const uint8_t tx_byte = tx_blob[byte_idx];
          byte ^= tx_byte;
        }

      }

      {
        /// reduce hash down to 8 bytes
        uint64_t n[4];
        std::memcpy(n, hash.data, sizeof(n));
        for (auto &ni : n) {
          boost::endian::little_to_native_inplace(ni);
        }

        /// Note that byte (obviously) only affects the lower byte
        /// of height, but that should be sufficient in this case
        height = n[0] ^ n[1] ^ n[2] ^ n[3] ^ byte;
      }

    }

    return height;
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_perform_blockchain_test(const COMMAND_RPC_PERFORM_BLOCKCHAIN_TEST::request& req,
                                                   COMMAND_RPC_PERFORM_BLOCKCHAIN_TEST::response& res,
                                                   epee::json_rpc::error& error_resp,
                                                   const connection_context* ctx)
  {
    PERF_TIMER(on_perform_blockchain_test);


    uint64_t max_height = req.max_height;
    uint64_t seed = req.seed;

    if (m_core.get_current_blockchain_height() <= max_height) {
      error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
      res.status = "Requested block height too big.";
      return true;
    }

    uint64_t res_height = perform_blockchain_test_routine(m_core, max_height, seed);

    res.status = CORE_RPC_STATUS_OK;
    res.res_height = res_height;

    return true;
  }

  namespace {
    struct version_printer { const std::array<int, 3> &v; };
    std::ostream &operator<<(std::ostream &o, const version_printer &vp) { return o << vp.v[0] << '.' << vp.v[1] << '.' << vp.v[2]; }

    // Handles a ping.  Returns true if the ping was significant (i.e. first ping after startup, or
    // after the ping had expired).
    template <typename Response>
    bool handle_ping(const std::array<int, 3> &cur_version, const std::array<int, 3> &required, const char *name, std::atomic<std::time_t> &update, time_t lifetime, Response &res)
    {
      if (cur_version < required) {
        std::ostringstream status;
        status << "Outdated " << name << ". Current: " << version_printer{cur_version} << " Required: " << version_printer{required};
        res.status = status.str();
        MERROR(res.status);
      } else {
        res.status = "OK";
        auto now = std::time(nullptr);
        auto old = update.exchange(now);
        if (old + lifetime < now) { // Print loudly for the first ping after startup/expiry
          MGINFO_GREEN("Received ping from " << name << " " << version_printer{cur_version});
          return true;
        }
        MDEBUG("Accepted ping from " << name << " " << version_printer{cur_version});
      }
      return false;
    }
  }

  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_storage_server_ping(const COMMAND_RPC_STORAGE_SERVER_PING::request& req,
                                               COMMAND_RPC_STORAGE_SERVER_PING::response& res,
                                               epee::json_rpc::error&,
                                               const connection_context*)
  {
    if (handle_ping({req.version_major, req.version_minor, req.version_patch}, service_nodes::MIN_STORAGE_SERVER_VERSION,
          "Storage Server", m_core.m_last_storage_server_ping, STORAGE_SERVER_PING_LIFETIME, res))
    {
      m_core.reset_proof_interval();

      m_core.m_storage_lmq_port = req.storage_lmq_port;
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_lokinet_ping(const COMMAND_RPC_LOKINET_PING::request& req,
                                        COMMAND_RPC_LOKINET_PING::response& res,
                                        epee::json_rpc::error&,
                                        const connection_context*)
  {
    if (handle_ping(req.version, service_nodes::MIN_LOKINET_VERSION,
          "Lokinet", m_core.m_last_lokinet_ping, LOKINET_PING_LIFETIME, res))
      m_core.reset_proof_interval();
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_staking_requirement(const COMMAND_RPC_GET_STAKING_REQUIREMENT::request& req, COMMAND_RPC_GET_STAKING_REQUIREMENT::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    PERF_TIMER(on_get_staking_requirement);
    res.height = req.height > 0 ? req.height : m_core.get_current_blockchain_height();

    res.staking_requirement = service_nodes::get_staking_requirement(m_core.get_nettype(), res.height, m_core.get_hard_fork_version(res.height));
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  static bool exceeds_quantity_limit(const core_rpc_server::connection_context *ctx, epee::json_rpc::error &error_resp, bool restricted, size_t count, size_t max, char const *container_name = nullptr)
  {
    if (ctx && restricted)
    {
      if (count > max)
      {
        error_resp.code     = CORE_RPC_ERROR_CODE_WRONG_PARAM;
        error_resp.message  = "Number of requested entries ";
        if (container_name)
        {
          error_resp.message += "in ";
          error_resp.message += container_name;
          error_resp.message += " ";
        }

        error_resp.message += "greater than the allowed limit: ";
        error_resp.message += std::to_string(max);
        error_resp.message += ", requested: ";
        error_resp.message += std::to_string(count);
        return true;
      }
    }
    return false;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_checkpoints(const COMMAND_RPC_GET_CHECKPOINTS::request& req, COMMAND_RPC_GET_CHECKPOINTS::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    bool bootstrap_daemon_connection_failure = false;
    if (use_bootstrap_daemon_if_necessary<COMMAND_RPC_GET_CHECKPOINTS>(invoke_http_mode::JON_RPC, "get_checkpoints", req, res, bootstrap_daemon_connection_failure))
      return bootstrap_daemon_connection_failure;

    if (exceeds_quantity_limit(ctx, error_resp, m_restricted, req.count, COMMAND_RPC_GET_CHECKPOINTS_MAX_COUNT))
      return false;

    res.status             = CORE_RPC_STATUS_OK;
    BlockchainDB const &db = m_core.get_blockchain_storage().get_db();

    std::vector<checkpoint_t> checkpoints;
    if (req.start_height == COMMAND_RPC_GET_CHECKPOINTS::HEIGHT_SENTINEL_VALUE &&
        req.end_height   == COMMAND_RPC_GET_CHECKPOINTS::HEIGHT_SENTINEL_VALUE)
    {
      checkpoint_t top_checkpoint;
      if (db.get_top_checkpoint(top_checkpoint))
        checkpoints = db.get_checkpoints_range(top_checkpoint.height, 0, req.count);
    }
    else if (req.start_height == COMMAND_RPC_GET_CHECKPOINTS::HEIGHT_SENTINEL_VALUE)
    {
      checkpoints = db.get_checkpoints_range(req.end_height, 0, req.count);
    }
    else if (req.end_height == COMMAND_RPC_GET_CHECKPOINTS::HEIGHT_SENTINEL_VALUE)
    {
      checkpoints = db.get_checkpoints_range(req.start_height, UINT64_MAX, req.count);
    }
    else
    {
      checkpoints = db.get_checkpoints_range(req.start_height, req.end_height);
    }

    res.checkpoints.reserve(checkpoints.size());
    for (checkpoint_t const &checkpoint : checkpoints)
      res.checkpoints.push_back(checkpoint);

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_get_service_nodes_state_changes(const COMMAND_RPC_GET_SN_STATE_CHANGES::request& req, COMMAND_RPC_GET_SN_STATE_CHANGES::response& res, epee::json_rpc::error& error_resp, const connection_context *ctx)
  {
    using blob_t = cryptonote::blobdata;
    using block_pair_t = std::pair<blob_t, block>;
    std::vector<block_pair_t> blocks;

    const auto& db = m_core.get_blockchain_storage();
    const uint64_t current_height = db.get_current_blockchain_height();

    uint64_t end_height;
    if (req.end_height == COMMAND_RPC_GET_SN_STATE_CHANGES::HEIGHT_SENTINEL_VALUE) {
      // current height is the block being mined, so exclude it from the results
      end_height = current_height - 1;
    } else {
      end_height = req.end_height;
    }

    if (end_height < req.start_height){
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "The provided end_height needs to be higher than start_height";
      return false;
    }

    if (!db.get_blocks(req.start_height, end_height - req.start_height + 1, blocks)) {
      error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
      error_resp.message = "Could not query block at requested height: " + std::to_string(req.start_height);
      return false;
    }

    res.start_height = req.start_height;
    res.end_height = end_height;

    std::vector<blob_t> blobs;
    std::vector<crypto::hash> missed_ids;
    for (const auto& block : blocks)
    {
      blobs.clear();
      if (!db.get_transactions_blobs(block.second.tx_hashes, blobs, missed_ids))
      {
        MERROR("Could not query block at requested height: " << cryptonote::get_block_height(block.second));
        continue;
      }
      const uint8_t hard_fork_version = block.second.major_version;
      for (const auto& blob : blobs)
      {
        cryptonote::transaction tx;
        if (!cryptonote::parse_and_validate_tx_from_blob(blob, tx))
        {
          MERROR("tx could not be validated from blob, possibly corrupt blockchain");
          continue;
        }
        if (tx.type == cryptonote::txtype::state_change)
        {
          cryptonote::tx_extra_service_node_state_change state_change;
          if (!cryptonote::get_service_node_state_change_from_tx_extra(tx.extra, state_change, hard_fork_version))
          {
            LOG_ERROR("Could not get state change from tx, possibly corrupt tx, hf_version "<< std::to_string(hard_fork_version));
            continue;
          }

          switch(state_change.state) {
            case service_nodes::new_state::deregister:
              res.total_deregister++;
              break;

            case service_nodes::new_state::decommission:
              res.total_decommission++;
              break;

            case service_nodes::new_state::recommission:
              res.total_recommission++;
              break;

            case service_nodes::new_state::ip_change_penalty:
              res.total_ip_change_penalty++;
              break;

            default:
              MERROR("Unhandled state in on_get_service_nodes_state_changes");
              break;
          }
        }

        if (tx.type == cryptonote::txtype::key_image_unlock)
        {
          res.total_unlock++;
        }
      }
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_report_peer_storage_server_status(const COMMAND_RPC_REPORT_PEER_SS_STATUS::request& req,
                                                             COMMAND_RPC_REPORT_PEER_SS_STATUS::response& res,
                                                             epee::json_rpc::error& error_resp,
                                                             const connection_context* ctx)
  {
    crypto::public_key pubkey;
    if (!string_tools::hex_to_pod(req.pubkey, pubkey)) {
      MERROR("Could not parse public key: " << req.pubkey);
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Could not parse public key";
      return false;
    }

    if (req.type == "reachability") {

      if (!m_core.set_storage_server_peer_reachable(pubkey, req.passed)) {
        error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
        error_resp.message = "Pubkey not found";
        return false;
      }
    } else {
      error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
      error_resp.message = "Unknown status type";
      return false;
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_test_trigger_p2p_resync(const COMMAND_RPC_TEST_TRIGGER_P2P_RESYNC::request& req,
                                                   COMMAND_RPC_TEST_TRIGGER_P2P_RESYNC::response& res,
                                                   epee::json_rpc::error& error_resp,
                                                   const connection_context* ctx)
  {
    m_p2p.reset_peer_handshake_timer();
    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_lns_names_to_owners(const COMMAND_RPC_LNS_NAMES_TO_OWNERS::request &req, COMMAND_RPC_LNS_NAMES_TO_OWNERS::response &res, epee::json_rpc::error &error_resp, const connection_context *ctx)
  {
    if (exceeds_quantity_limit(ctx, error_resp, m_restricted, req.entries.size(), COMMAND_RPC_LNS_NAMES_TO_OWNERS::MAX_REQUEST_ENTRIES))
      return false;

    lns::name_system_db &db = m_core.get_blockchain_storage().name_system_db();
    for (size_t request_index = 0; request_index < req.entries.size(); request_index++)
    {
      COMMAND_RPC_LNS_NAMES_TO_OWNERS::request_entry const &request = req.entries[request_index];
      if (exceeds_quantity_limit(ctx, error_resp, m_restricted, request.types.size(), COMMAND_RPC_LNS_NAMES_TO_OWNERS::MAX_TYPE_REQUEST_ENTRIES, "types"))
        return false;

      std::vector<lns::mapping_record> records = db.get_mappings(request.types, request.name_hash);
      for (auto const &record : records)
      {
        res.entries.emplace_back();
        COMMAND_RPC_LNS_NAMES_TO_OWNERS::response_entry &entry = res.entries.back();
        entry.entry_index                                      = request_index;
        entry.type                                             = static_cast<uint16_t>(record.type);
        entry.name_hash                                        = record.name_hash;
        entry.owner                                            = record.owner.to_string(nettype());
        if (record.backup_owner) entry.backup_owner            = record.backup_owner.to_string(nettype());
        entry.encrypted_value                                  = epee::to_hex::string(record.encrypted_value.to_span());
        entry.register_height                                  = record.register_height;
        entry.update_height                                    = record.update_height;
        entry.txid                                             = epee::string_tools::pod_to_hex(record.txid);
        if (record.prev_txid) entry.prev_txid                  = epee::string_tools::pod_to_hex(record.prev_txid);
      }
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool core_rpc_server::on_lns_owners_to_names(const COMMAND_RPC_LNS_OWNERS_TO_NAMES::request &req, COMMAND_RPC_LNS_OWNERS_TO_NAMES::response &res, epee::json_rpc::error &error_resp, const connection_context *ctx)
  {
    if (exceeds_quantity_limit(ctx, error_resp, m_restricted, req.entries.size(), COMMAND_RPC_LNS_OWNERS_TO_NAMES::MAX_REQUEST_ENTRIES))
      return false;

    std::unordered_map<lns::generic_owner, size_t> owner_to_request_index;
    std::vector<lns::generic_owner> owners;

    owners.reserve(req.entries.size());
    for (size_t request_index = 0; request_index < req.entries.size(); request_index++)
    {
      std::string const &owner     = req.entries[request_index];
      lns::generic_owner lns_owner = {};
      if (!lns::parse_owner_to_generic_owner(m_core.get_nettype(), owner, lns_owner, &error_resp.message))
      {
        error_resp.code = CORE_RPC_ERROR_CODE_WRONG_PARAM;
        return false;
      }

      // TODO(loki): We now serialize both owner and backup_owner, since if
      // we specify an owner that is backup owner, we don't show the (other)
      // owner. For RPC compatibility we keep the request_index around until the
      // next hard fork (16)
      owners.push_back(lns_owner);
      owner_to_request_index[lns_owner] = request_index;
    }

    lns::name_system_db &db = m_core.get_blockchain_storage().name_system_db();
    std::vector<lns::mapping_record> records = db.get_mappings_by_owners(owners);
    for (auto &record : records)
    {
      res.entries.emplace_back();
      COMMAND_RPC_LNS_OWNERS_TO_NAMES::response_entry &entry = res.entries.back();

      auto it = owner_to_request_index.find(record.owner);
      if (it == owner_to_request_index.end())
      {
        error_resp.code    = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
        error_resp.message = "Owner=" + record.owner.to_string(nettype()) + ", could not be mapped back a index in the request 'entries' array";
        return false;
      }

      entry.request_index   = it->second;
      entry.type            = static_cast<uint16_t>(record.type);
      entry.name_hash       = std::move(record.name_hash);
      if (record.owner) entry.owner = record.owner.to_string(nettype());
      if (record.backup_owner) entry.backup_owner = record.backup_owner.to_string(nettype());
      entry.encrypted_value = epee::to_hex::string(record.encrypted_value.to_span());
      entry.register_height = record.register_height;
      entry.update_height   = record.update_height;
      entry.txid            = epee::string_tools::pod_to_hex(record.txid);
      if (record.prev_txid) entry.prev_txid = epee::string_tools::pod_to_hex(record.prev_txid);
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
  }
}  // namespace cryptonote
