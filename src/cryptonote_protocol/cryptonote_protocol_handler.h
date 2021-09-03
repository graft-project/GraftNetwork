/// @file
/// @author rfree (current maintainer/user in monero.cc project - most of code is from CryptoNote)
/// @brief This is the original cryptonote protocol network-events handler, modified by us

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

#pragma once

#include <boost/program_options/variables_map.hpp>
#include <string>
#include <unordered_map>

#include "math_helper.h"
#include "storages/levin_abstract_invoke2.h"
#include "warnings.h"
#include "cryptonote_protocol_defs.h"
#include "cryptonote_protocol_handler_common.h"
#include "block_queue.h"
#include "common/perf_timer.h"
#include "cryptonote_basic/connection_context.h"
#include "cryptonote_basic/cryptonote_stat_info.h"
#include <boost/circular_buffer.hpp>

PUSH_WARNINGS
DISABLE_VS_WARNINGS(4355)

#define CURRENCY_PROTOCOL_MAX_OBJECT_REQUEST_COUNT 500
#define CURRENCY_PROTOCOL_MAX_TXS_REQUEST_COUNT 5000

namespace cryptonote
{
  using namespace std::literals;

  template<class t_core>
  class t_cryptonote_protocol_handler:  public i_cryptonote_protocol
  {
  public:
    typedef cryptonote_connection_context connection_context;
    typedef core_stat_info stat_info;
    typedef t_cryptonote_protocol_handler<t_core> cryptonote_protocol_handler;
    typedef CORE_SYNC_DATA payload_type;

    t_cryptonote_protocol_handler(t_core& rcore, nodetool::i_p2p_endpoint<connection_context>* p_net_layout, bool offline = false);

    BEGIN_INVOKE_MAP2(cryptonote_protocol_handler)
      HANDLE_NOTIFY_T2(NOTIFY_NEW_TRANSACTIONS, handle_notify_new_transactions)
      HANDLE_NOTIFY_T2(NOTIFY_REQUEST_GET_BLOCKS, handle_request_get_blocks)
      HANDLE_NOTIFY_T2(NOTIFY_RESPONSE_GET_BLOCKS, handle_response_get_blocks)
      HANDLE_NOTIFY_T2(NOTIFY_REQUEST_GET_TXS, handle_request_get_txs)
      HANDLE_NOTIFY_T2(NOTIFY_REQUEST_CHAIN, handle_request_chain)
      HANDLE_NOTIFY_T2(NOTIFY_RESPONSE_CHAIN_ENTRY, handle_response_chain_entry)
      HANDLE_NOTIFY_T2(NOTIFY_NEW_FLUFFY_BLOCK, handle_notify_new_fluffy_block)
      HANDLE_NOTIFY_T2(NOTIFY_REQUEST_FLUFFY_MISSING_TX, handle_request_fluffy_missing_tx)
      HANDLE_NOTIFY_T2(NOTIFY_UPTIME_PROOF, handle_uptime_proof)
      HANDLE_NOTIFY_T2(NOTIFY_NEW_SERVICE_NODE_VOTE, handle_notify_new_service_node_vote)
      HANDLE_NOTIFY_T2(NOTIFY_REQUEST_BLOCK_BLINKS, handle_request_block_blinks)
      HANDLE_NOTIFY_T2(NOTIFY_RESPONSE_BLOCK_BLINKS, handle_response_block_blinks)
    END_INVOKE_MAP2()

    bool on_idle();
    bool init(const boost::program_options::variables_map& vm);
    bool deinit();
    void set_p2p_endpoint(nodetool::i_p2p_endpoint<connection_context>* p2p);
    //bool process_handshake_data(const blobdata& data, cryptonote_connection_context& context);
    bool process_payload_sync_data(CORE_SYNC_DATA&& hshd, cryptonote_connection_context& context, bool is_inital);
    bool get_payload_sync_data(blobdata& data);
    bool get_payload_sync_data(CORE_SYNC_DATA& hshd);
    bool get_stat_info(core_stat_info& stat_inf);
    bool on_callback(cryptonote_connection_context& context);
    t_core& get_core(){return m_core;}
    bool is_synchronized(){return m_synchronized;}
    void log_connections();
    std::list<connection_info> get_connections();
    const block_queue &get_block_queue() const { return m_block_queue; }
    void stop();
    void on_connection_close(cryptonote_connection_context &context);
    void set_max_out_peers(unsigned int max) { m_max_out_peers = max; }
    void set_no_sync(bool value) { m_no_sync = value; }
    std::string get_peers_overview() const;
    std::pair<uint32_t, uint32_t> get_next_needed_pruning_stripe() const;
    bool needs_new_sync_connections() const;
  private:
    //----------------- commands handlers ----------------------------------------------
    int handle_notify_new_transactions(int command, NOTIFY_NEW_TRANSACTIONS::request& arg, cryptonote_connection_context& context);
    int handle_request_get_blocks(int command, NOTIFY_REQUEST_GET_BLOCKS::request& arg, cryptonote_connection_context& context);
    int handle_response_get_blocks(int command, NOTIFY_RESPONSE_GET_BLOCKS::request& arg, cryptonote_connection_context& context);
    int handle_request_get_txs(int command, NOTIFY_REQUEST_GET_TXS::request& arg, cryptonote_connection_context& context);
    int handle_request_chain(int command, NOTIFY_REQUEST_CHAIN::request& arg, cryptonote_connection_context& context);
    int handle_response_chain_entry(int command, NOTIFY_RESPONSE_CHAIN_ENTRY::request& arg, cryptonote_connection_context& context);
    int handle_notify_new_fluffy_block(int command, NOTIFY_NEW_FLUFFY_BLOCK::request& arg, cryptonote_connection_context& context);
    int handle_request_fluffy_missing_tx(int command, NOTIFY_REQUEST_FLUFFY_MISSING_TX::request& arg, cryptonote_connection_context& context);
    int handle_uptime_proof(int command, NOTIFY_UPTIME_PROOF::request& arg, cryptonote_connection_context& context);
    int handle_notify_new_service_node_vote(int command, NOTIFY_NEW_SERVICE_NODE_VOTE::request& arg, cryptonote_connection_context& context);
    int handle_request_block_blinks(int command, NOTIFY_REQUEST_BLOCK_BLINKS::request& arg, cryptonote_connection_context& context);
    int handle_response_block_blinks(int command, NOTIFY_RESPONSE_BLOCK_BLINKS::request& arg, cryptonote_connection_context& context);

    //----------------- i_bc_protocol_layout ---------------------------------------
    template<class T>
    bool relay_to_synchronized_peers(typename T::request& arg, cryptonote_connection_context& exclude_context)
    {
      LOG_PRINT_L2("[" << epee::net_utils::print_connection_context_short(exclude_context) << "] post relay " << typeid(T).name() << " -->");
      std::vector<std::pair<epee::net_utils::zone, boost::uuids::uuid>> connections;
      m_p2p->for_each_connection([&exclude_context, &connections](connection_context& context, nodetool::peerid_type peer_id, uint32_t support_flags)
      {
        if (context.m_state > cryptonote_connection_context::state_synchronizing)
        {
          epee::net_utils::zone zone = context.m_remote_address.get_zone();
          if (peer_id && exclude_context.m_connection_id != context.m_connection_id)
            connections.push_back({zone, context.m_connection_id});
        }
        return true;
      });

      if (connections.size())
      {
        std::string arg_buff;
        epee::serialization::store_t_to_binary(arg, arg_buff);
        return m_p2p->relay_notify_to_list(T::ID, epee::strspan<uint8_t>(arg_buff), std::move(connections));
      }

      return true;
    }

    virtual bool relay_block(NOTIFY_NEW_FLUFFY_BLOCK::request& arg, cryptonote_connection_context& exclude_context);
    virtual bool relay_transactions(NOTIFY_NEW_TRANSACTIONS::request& arg, cryptonote_connection_context& exclude_context);
    virtual bool relay_uptime_proof(NOTIFY_UPTIME_PROOF::request& arg, cryptonote_connection_context& exclude_context);
    virtual bool relay_service_node_votes(NOTIFY_NEW_SERVICE_NODE_VOTE::request& arg, cryptonote_connection_context& exclude_context);
    //----------------------------------------------------------------------------------
    //bool get_payload_sync_data(HANDSHAKE_DATA::request& hshd, cryptonote_connection_context& context);
    bool should_drop_connection(cryptonote_connection_context& context, uint32_t next_stripe);
    bool request_missing_objects(cryptonote_connection_context& context, bool check_having_blocks, bool force_next_span = false);
    size_t get_synchronizing_connections_count();
    bool on_connection_synchronized();
    bool should_download_next_span(cryptonote_connection_context& context, bool standby);
    void drop_connection(cryptonote_connection_context &context, bool add_fail, bool flush_all_spans);
    bool kick_idle_peers();
    bool check_standby_peers();
    bool update_sync_search();
    int try_add_next_blocks(cryptonote_connection_context &context);
    void notify_new_stripe(cryptonote_connection_context &context, uint32_t stripe);
    void skip_unneeded_hashes(cryptonote_connection_context& context, bool check_block_queue) const;

    t_core& m_core;

    nodetool::p2p_endpoint_stub<connection_context> m_p2p_stub;
    nodetool::i_p2p_endpoint<connection_context>* m_p2p;
    std::atomic<uint32_t> m_syncronized_connections_count;
    std::atomic<bool> m_synchronized;
    std::atomic<bool> m_stopping;
    std::atomic<bool> m_no_sync;
    boost::mutex m_sync_lock;
    block_queue m_block_queue;
    epee::math_helper::periodic_task m_idle_peer_kicker{30s};
    epee::math_helper::periodic_task m_standby_checker{100ms};
    epee::math_helper::periodic_task m_sync_search_checker{101s};
    std::atomic<unsigned int> m_max_out_peers;
    tools::PerformanceTimer m_sync_timer, m_add_timer;
    uint64_t m_last_add_end_time;
    uint64_t m_sync_spans_downloaded, m_sync_old_spans_downloaded, m_sync_bad_spans_downloaded;
    uint64_t m_sync_download_chain_size, m_sync_download_objects_size;
    size_t m_block_download_max_size;

    boost::mutex m_buffer_mutex;
    boost::circular_buffer<size_t> m_avg_buffer = boost::circular_buffer<size_t>(10);

    template<class t_parameter>
      bool post_notify(typename t_parameter::request& arg, cryptonote_connection_context& context)
      {
        LOG_PRINT_L2("[" << epee::net_utils::print_connection_context_short(context) << "] post " << typeid(t_parameter).name() << " -->");
        std::string blob;
        epee::serialization::store_t_to_binary(arg, blob);
        return m_p2p->invoke_notify_to_peer(t_parameter::ID, epee::strspan<uint8_t>(blob), context);
      }
  };

} // namespace

POP_WARNINGS
