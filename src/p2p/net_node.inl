// Copyright (c) 2018, The Graft Project
// Copyright (c) 2014-2017, The Monero Project
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

// IP blocking adapted from Boolberry

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <boost/algorithm/string/join.hpp> // for logging

#include "version.h"
#include "string_tools.h"
#include "common/util.h"
#include "common/dns_utils.h"
#include "net/net_helper.h"
#include "math_helper.h"
#include "p2p_protocol_defs.h"
#include "net_peerlist_boost_serialization.h"
#include "net/local_ip.h"
#include "crypto/crypto.h"
#include "storages/levin_abstract_invoke2.h"
#include "storages/http_abstract_invoke.h"

// We have to look for miniupnpc headers in different places, dependent on if its compiled or external
#ifdef UPNP_STATIC
  #include <miniupnpc/miniupnpc.h>
  #include <miniupnpc/upnpcommands.h>
  #include <miniupnpc/upnperrors.h>
#else
  #include "miniupnpc.h"
  #include "upnpcommands.h"
  #include "upnperrors.h"
#endif

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "net.p2p"

#define NET_MAKE_IP(b1,b2,b3,b4)  ((LPARAM)(((DWORD)(b1)<<24)+((DWORD)(b2)<<16)+((DWORD)(b3)<<8)+((DWORD)(b4))))

#define MIN_WANTED_SEED_NODES 12

#define MAX_TUNNEL_PEERS (3u)
#define REQUEST_CACHE_TIME 2 * 60 * 1000
#define HOP_RETRIES_MULTIPLIER 2

namespace nodetool
{
  namespace
  {
    const int64_t default_limit_up = 2048;
    const int64_t default_limit_down = 8192;
    const command_line::arg_descriptor<std::string> arg_p2p_bind_ip        = {"p2p-bind-ip", "Interface for p2p network protocol", "0.0.0.0"};
    const command_line::arg_descriptor<std::string> arg_p2p_bind_port = {
        "p2p-bind-port"
      , "Port for p2p network protocol"
      , std::to_string(config::P2P_DEFAULT_PORT)
      };
    const command_line::arg_descriptor<std::string> arg_testnet_p2p_bind_port = {
        "testnet-p2p-bind-port"
      , "Port for testnet p2p network protocol"
      , std::to_string(config::testnet::P2P_DEFAULT_PORT)
      };
    const command_line::arg_descriptor<uint32_t>    arg_p2p_external_port  = {"p2p-external-port", "External port for p2p network protocol (if port forwarding used with NAT)", 0};
    const command_line::arg_descriptor<bool>        arg_p2p_allow_local_ip = {"allow-local-ip", "Allow local ip add to peer list, mostly in debug purposes"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_peer   = {"add-peer", "Manually add peer to local peerlist"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_priority_node   = {"add-priority-node", "Specify list of peers to connect to and attempt to keep the connection open"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_exclusive_node   = {"add-exclusive-node", "Specify list of peers to connect to only."
                                                                                                  " If this option is given the options add-priority-node and seed-node are ignored"};
    const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_seed_node   = {"seed-node", "Specify list of seednodes, same way as --add-exclusive-node. Only effective with testnet option"};
    const command_line::arg_descriptor<bool> arg_p2p_hide_my_port   =    {"hide-my-port", "Do not announce yourself as peerlist candidate", false, true};

    const command_line::arg_descriptor<bool>        arg_no_igd  = {"no-igd", "Disable UPnP port mapping"};
    const command_line::arg_descriptor<bool>        arg_offline = {"offline", "Do not listen for peers, nor connect to any"};
    const command_line::arg_descriptor<int64_t>     arg_out_peers = {"out-peers", "set max number of out peers", -1};
    const command_line::arg_descriptor<int> arg_tos_flag = {"tos-flag", "set TOS flag", -1};

    const command_line::arg_descriptor<int64_t> arg_limit_rate_up = {"limit-rate-up", "set limit-rate-up [kB/s]", -1};
    const command_line::arg_descriptor<int64_t> arg_limit_rate_down = {"limit-rate-down", "set limit-rate-down [kB/s]", -1};
    const command_line::arg_descriptor<int64_t> arg_limit_rate = {"limit-rate", "set limit-rate [kB/s]", -1};

    const command_line::arg_descriptor<bool> arg_save_graph = {"save-graph", "Save data for dr monero", false};
    const command_line::arg_descriptor<Uuid> arg_p2p_net_id = {"net-id", "The way to replace hardcoded NETWORK_ID. Effective only with --testnet, ex.: 'net-id = 54686520-4172-7420-6f77-205761722037'"};
  }

  inline void append_net_address(std::vector<epee::net_utils::network_address> & seed_nodes, std::string const & addr);
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_p2p_bind_ip);
    command_line::add_arg(desc, arg_p2p_bind_port, false);
    command_line::add_arg(desc, arg_testnet_p2p_bind_port, false);
    command_line::add_arg(desc, arg_p2p_external_port);
    command_line::add_arg(desc, arg_p2p_allow_local_ip);
    command_line::add_arg(desc, arg_p2p_add_peer);
    command_line::add_arg(desc, arg_p2p_add_priority_node);
    command_line::add_arg(desc, arg_p2p_add_exclusive_node);
    command_line::add_arg(desc, arg_p2p_seed_node);
    command_line::add_arg(desc, arg_p2p_hide_my_port);
    command_line::add_arg(desc, arg_no_igd);
    command_line::add_arg(desc, arg_offline);
    command_line::add_arg(desc, arg_out_peers);
    command_line::add_arg(desc, arg_tos_flag);
    command_line::add_arg(desc, arg_limit_rate_up);
    command_line::add_arg(desc, arg_limit_rate_down);
    command_line::add_arg(desc, arg_limit_rate);
    command_line::add_arg(desc, arg_save_graph);
    command_line::add_arg(desc, arg_p2p_net_id);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::init_config()
  {
    //
    TRY_ENTRY();
    std::string state_file_path = m_config_folder + "/" + P2P_NET_DATA_FILENAME;
    std::ifstream p2p_data;
    p2p_data.open( state_file_path , std::ios_base::binary | std::ios_base::in);
    if(!p2p_data.fail())
    {
      try
      {
        // first try reading in portable mode
        boost::archive::portable_binary_iarchive a(p2p_data);
        a >> *this;
      }
      catch (...)
      {
        // if failed, try reading in unportable mode
        boost::filesystem::copy_file(state_file_path, state_file_path + ".unportable", boost::filesystem::copy_option::overwrite_if_exists);
        p2p_data.close();
        p2p_data.open( state_file_path , std::ios_base::binary | std::ios_base::in);
        if(!p2p_data.fail())
        {
          try
          {
            boost::archive::binary_iarchive a(p2p_data);
            a >> *this;
          }
          catch (const std::exception &e)
          {
            MWARNING("Failed to load p2p config file, falling back to default config");
            m_peerlist = peerlist_manager(); // it was probably half clobbered by the failed load
            make_default_config();
          }
        }
        else
        {
          make_default_config();
        }
      }
    }else
    {
      make_default_config();
    }

    // always recreate a new peer id
    make_default_peer_id();

    //at this moment we have hardcoded config
    m_config.m_net_config.handshake_interval = P2P_DEFAULT_HANDSHAKE_INTERVAL;
    m_config.m_net_config.packet_max_size = P2P_DEFAULT_PACKET_MAX_SIZE; //20 MB limit
    m_config.m_net_config.config_id = 0; // initial config
    m_config.m_net_config.connection_timeout = P2P_DEFAULT_CONNECTION_TIMEOUT;
    m_config.m_net_config.ping_connection_timeout = P2P_DEFAULT_PING_CONNECTION_TIMEOUT;
    m_config.m_net_config.send_peerlist_sz = P2P_DEFAULT_PEERS_IN_HANDSHAKE;
    m_config.m_support_flags = P2P_SUPPORT_FLAGS;

    m_first_connection_maker_call = true;
    CATCH_ENTRY_L0("node_server::init_config", false);
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::for_each_connection(std::function<bool(typename t_payload_net_handler::connection_context&, peerid_type, uint32_t)> f)
  {
    m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& cntx){
      return f(cntx, cntx.peer_id, cntx.support_flags);
    });
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::for_connection(const boost::uuids::uuid &connection_id, std::function<bool(typename t_payload_net_handler::connection_context&, peerid_type, uint32_t)> f)
  {
    return m_net_server.get_config_object().for_connection(connection_id, [&](p2p_connection_context& cntx){
      return f(cntx, cntx.peer_id, cntx.support_flags);
    });
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_remote_host_allowed(const epee::net_utils::network_address &address)
  {
    CRITICAL_REGION_LOCAL(m_blocked_hosts_lock);
    auto it = m_blocked_hosts.find(address.host_str());
    if(it == m_blocked_hosts.end())
      return true;
    if(time(nullptr) >= it->second)
    {
      m_blocked_hosts.erase(it);
      MCLOG_CYAN(el::Level::Info, "global", "Host " << address.host_str() << " unblocked.");
      return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_default_peer_id()
  {
    m_config.m_peer_id  = crypto::rand<uint64_t>();
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_default_config()
  {
    return make_default_peer_id();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::block_host(const epee::net_utils::network_address &addr, time_t seconds)
  {
    CRITICAL_REGION_LOCAL(m_blocked_hosts_lock);
    m_blocked_hosts[addr.host_str()] = time(nullptr) + seconds;

    // drop any connection to that IP
    std::list<boost::uuids::uuid> conns;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if (cntxt.m_remote_address.is_same_host(addr))
      {
        conns.push_back(cntxt.m_connection_id);
      }
      return true;
    });
    for (const auto &c: conns)
      m_net_server.get_config_object().close(c);

    MCLOG_CYAN(el::Level::Info, "global", "Host " << addr.host_str() << " blocked.");
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::unblock_host(const epee::net_utils::network_address &address)
  {
    CRITICAL_REGION_LOCAL(m_blocked_hosts_lock);
    auto i = m_blocked_hosts.find(address.host_str());
    if (i == m_blocked_hosts.end())
      return false;
    m_blocked_hosts.erase(i);
    MCLOG_CYAN(el::Level::Info, "global", "Host " << address.host_str() << " unblocked.");
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::add_host_fail(const epee::net_utils::network_address &address)
  {
    CRITICAL_REGION_LOCAL(m_host_fails_score_lock);
    uint64_t fails = ++m_host_fails_score[address.host_str()];
    MDEBUG("Host " << address.host_str() << " fail score=" << fails);
    if(fails > P2P_IP_FAILS_BEFORE_BLOCK)
    {
      auto it = m_host_fails_score.find(address.host_str());
      CHECK_AND_ASSERT_MES(it != m_host_fails_score.end(), false, "internal error");
      it->second = P2P_IP_FAILS_BEFORE_BLOCK/2;
      block_host(address);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::parse_peer_from_string(epee::net_utils::network_address& pe, const std::string& node_addr, uint16_t default_port)
  {
    return epee::net_utils::create_network_address(pe, node_addr, default_port);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::handle_command_line(
      const boost::program_options::variables_map& vm
    )
  {
    auto p2p_bind_arg = m_testnet ? arg_testnet_p2p_bind_port : arg_p2p_bind_port;

    m_bind_ip = command_line::get_arg(vm, arg_p2p_bind_ip);
    m_port = command_line::get_arg(vm, p2p_bind_arg);
    m_external_port = command_line::get_arg(vm, arg_p2p_external_port);
    m_allow_local_ip = command_line::get_arg(vm, arg_p2p_allow_local_ip);
    m_no_igd = command_line::get_arg(vm, arg_no_igd);
    m_offline = command_line::get_arg(vm, arg_offline);

    if (command_line::has_arg(vm, arg_p2p_add_peer))
    {
      std::vector<std::string> perrs = command_line::get_arg(vm, arg_p2p_add_peer);
      for(const std::string& pr_str: perrs)
      {
        nodetool::peerlist_entry pe = AUTO_VAL_INIT(pe);
        pe.id = crypto::rand<uint64_t>();
        const uint16_t default_port = m_testnet ? ::config::testnet::P2P_DEFAULT_PORT : ::config::P2P_DEFAULT_PORT;
        bool r = parse_peer_from_string(pe.adr, pr_str, default_port);
        CHECK_AND_ASSERT_MES(r, false, "Failed to parse address from string: " << pr_str);
        m_command_line_peers.push_back(pe);
      }
    }

    if(command_line::has_arg(vm, arg_save_graph))
    {
      set_save_graph(true);
    }

    if (command_line::has_arg(vm,arg_p2p_add_exclusive_node))
    {
      if (!parse_peers_and_add_to_container(vm, arg_p2p_add_exclusive_node, m_exclusive_peers))
        return false;
    }

    if (command_line::has_arg(vm, arg_p2p_add_priority_node))
    {
      if (!parse_peers_and_add_to_container(vm, arg_p2p_add_priority_node, m_priority_peers))
        return false;
    }

    // TODO: remove "testnet-only" limitation?
    if (m_testnet && command_line::has_arg(vm, arg_p2p_seed_node))
    {
      // XXX: in some reasons command_line::has_arg always returns true for "vector" type args
      std::vector<std::string> seed_nodes = command_line::get_arg(vm, arg_p2p_seed_node);
      if (!seed_nodes.empty())
      {
        MINFO("Overwriting seed nodes from command-line");
        m_seed_nodes.clear();
        for (const std::string sn : seed_nodes)
          append_net_address(m_seed_nodes, sn);
        m_force_seed_nodes = true;
      }
    }

    if(command_line::has_arg(vm, arg_p2p_hide_my_port))
      m_hide_my_port = true;

    if ( !set_max_out_peers(vm, command_line::get_arg(vm, arg_out_peers) ) )
      return false;

    if ( !set_tos_flag(vm, command_line::get_arg(vm, arg_tos_flag) ) )
      return false;

    if ( !set_rate_up_limit(vm, command_line::get_arg(vm, arg_limit_rate_up) ) )
      return false;

    if ( !set_rate_down_limit(vm, command_line::get_arg(vm, arg_limit_rate_down) ) )
      return false;

    if ( !set_rate_limit(vm, command_line::get_arg(vm, arg_limit_rate) ) )
      return false;

    return true;
  }
  //-----------------------------------------------------------------------------------
  inline void append_net_address(
      std::vector<epee::net_utils::network_address> & seed_nodes
    , std::string const & addr
    )
  {
    using namespace boost::asio;

    size_t pos = addr.find_last_of(':');
    CHECK_AND_ASSERT_MES_NO_RET(std::string::npos != pos && addr.length() - 1 != pos && 0 != pos, "Failed to parse seed address from string: '" << addr << '\'');
    std::string host = addr.substr(0, pos);
    std::string port = addr.substr(pos + 1);

    io_service io_srv;
    ip::tcp::resolver resolver(io_srv);
    ip::tcp::resolver::query query(host, port, boost::asio::ip::tcp::resolver::query::canonical_name);
    boost::system::error_code ec;
    ip::tcp::resolver::iterator i = resolver.resolve(query, ec);
    CHECK_AND_ASSERT_MES_NO_RET(!ec, "Failed to resolve host name '" << host << "': " << ec.message() << ':' << ec.value());

    ip::tcp::resolver::iterator iend;
    for (; i != iend; ++i)
    {
      ip::tcp::endpoint endpoint = *i;
      if (endpoint.address().is_v4())
      {
        epee::net_utils::network_address na(new epee::net_utils::ipv4_network_address(boost::asio::detail::socket_ops::host_to_network_long(endpoint.address().to_v4().to_ulong()), endpoint.port()));
        seed_nodes.push_back(na);
        MINFO("Added seed node: " << na.str());
      }
      else
      {
        MWARNING("IPv6 unsupported, skip '" << host << "' -> " << endpoint.address().to_v6().to_string(ec));
        throw std::runtime_error("IPv6 unsupported");
      }
    }
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  std::set<std::string> node_server<t_payload_net_handler>::get_seed_nodes(const bool testnet) const
  {
    std::set<std::string> full_addrs;
    if (testnet)
    {
      // return embedded seed-nodes only if seed-nodes was not passed via command-line
      if (!m_force_seed_nodes)
      {
        full_addrs.insert("18.214.197.224:28680");
        full_addrs.insert("18.214.197.50:28680");
        full_addrs.insert("35.169.179.171:28680");
      }
    }
    else
    {
      full_addrs.insert("18.214.197.224:18980");
      full_addrs.insert("18.214.197.50:18980");
      full_addrs.insert("35.169.179.171:18980");
    }
    return full_addrs;
  }

  inline void assign_network_id(const boost::program_options::variables_map& vm, const bool testnet, Uuid& net_id)
  {
    Uuid id = ::config::NETWORK_ID;
    std::string hint = "main-net's";

    if(testnet)
    {
      if(command_line::has_arg(vm, arg_p2p_net_id))
      {
        const Uuid ext_id = command_line::get_arg(vm, arg_p2p_net_id);
        if(ext_id != Uuid())
        {
          if(ext_id == ::config::NETWORK_ID)
          {
            std::ostringstream m;
            m << "Fail on assigning network-id: an attempt to use main-net-network-id for test-net mode";
            throw std::runtime_error(m.str());
          }

          if(ext_id == ::config::testnet::NETWORK_ID)
          {
            std::ostringstream m;
            m << "Fail on assigning network-id: passinng test-net-network-id from outside looks too suspecious";
            throw std::runtime_error(m.str());
          }

          id = ext_id;
          hint = "overriden from external config";
        }
        else
        {
          id = ::config::testnet::NETWORK_ID;
          hint = "test-net's";
        }
      }
    }
    memcpy(&net_id, &id, 16);
    MDEBUG("NETWORK_ID: '" << net_id << "' (" << hint << ")" << std::endl);
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::init(const boost::program_options::variables_map& vm)
  {
    std::set<std::string> full_addrs;
    m_testnet = command_line::get_arg(vm, command_line::arg_testnet_on);
    assign_network_id(vm, m_testnet, m_network_id);
    if(m_testnet)
    {
      full_addrs = get_seed_nodes(true);
    }
    else
    {
      // for each hostname in the seed nodes list, attempt to DNS resolve and
      // add the result addresses as seed nodes
      // TODO: at some point add IPv6 support, but that won't be relevant
      // for some time yet.

      std::vector<std::vector<std::string>> dns_results;
      dns_results.resize(m_seed_nodes_list.size());

      std::list<boost::thread> dns_threads;
      uint64_t result_index = 0;
      for (const std::string& addr_str : m_seed_nodes_list)
      {
        boost::thread th = boost::thread([=, &dns_results, &addr_str]
        {
          MDEBUG("dns_threads[" << result_index << "] created for: " << addr_str);
          // TODO: care about dnssec avail/valid
          bool avail, valid;
          std::vector<std::string> addr_list;

          try
          {
            addr_list = tools::DNSResolver::instance().get_ipv4(addr_str, avail, valid);
            MDEBUG("dns_threads[" << result_index << "] DNS resolve done");
            boost::this_thread::interruption_point();
          }
          catch(const boost::thread_interrupted&)
          {
            // thread interruption request
            // even if we now have results, finish thread without setting
            // result variables, which are now out of scope in main thread
            MWARNING("dns_threads[" << result_index << "] interrupted");
            return;
          }

          MINFO("dns_threads[" << result_index << "] addr_str: " << addr_str << "  number of results: " << addr_list.size());
          dns_results[result_index] = addr_list;
        });

        dns_threads.push_back(std::move(th));
        ++result_index;
      }

      MDEBUG("dns_threads created, now waiting for completion or timeout of " << CRYPTONOTE_DNS_TIMEOUT_MS << "ms");
      boost::chrono::system_clock::time_point deadline = boost::chrono::system_clock::now() + boost::chrono::milliseconds(CRYPTONOTE_DNS_TIMEOUT_MS);
      uint64_t i = 0;
      for (boost::thread& th : dns_threads)
      {
        if (! th.try_join_until(deadline))
        {
          MWARNING("dns_threads[" << i << "] timed out, sending interrupt");
          th.interrupt();
        }
        ++i;
      }

      i = 0;
      for (const auto& result : dns_results)
      {
        MDEBUG("DNS lookup for " << m_seed_nodes_list[i] << ": " << result.size() << " results");
        // if no results for node, thread's lookup likely timed out
        if (result.size())
        {
          for (const auto& addr_string : result)
            full_addrs.insert(addr_string + ":18080");
        }
        ++i;
      }

      // append the fallback nodes if we have too few seed nodes to start with
      if (full_addrs.size() < MIN_WANTED_SEED_NODES)
      {
        if (full_addrs.empty())
          MINFO("DNS seed node lookup either timed out or failed, falling back to defaults");
        else
          MINFO("Not enough DNS seed nodes found, using fallback defaults too");

        for (const auto &peer: get_seed_nodes(false))
          full_addrs.insert(peer);
      }
    }

    for (const auto& full_addr : full_addrs)
    {
      append_net_address(m_seed_nodes, full_addr);
    }
    bool res = handle_command_line(vm);
    CHECK_AND_ASSERT_MES(res, false, "Failed to handle command line");

    std::vector<std::string> dbg_seed_nodes;
    for (const auto &na : m_seed_nodes) {
      dbg_seed_nodes.push_back(na.str());
    }
    MDEBUG("Using seed-nodes: " << boost::algorithm::join(dbg_seed_nodes, ", "));

    auto config_arg = m_testnet ? command_line::arg_testnet_data_dir : command_line::arg_data_dir;
    m_config_folder = command_line::get_arg(vm, config_arg);

    if ((!m_testnet && m_port != std::to_string(::config::P2P_DEFAULT_PORT))
        || (m_testnet && m_port != std::to_string(::config::testnet::P2P_DEFAULT_PORT))) {
      m_config_folder = m_config_folder + "/" + m_port;
    }

    res = init_config();
    CHECK_AND_ASSERT_MES(res, false, "Failed to init config.");

    res = m_peerlist.init(m_allow_local_ip);
    CHECK_AND_ASSERT_MES(res, false, "Failed to init peerlist.");


    for(auto& p: m_command_line_peers)
      m_peerlist.append_with_peer_white(p);

    //only in case if we really sure that we have external visible ip
    m_have_address = true;
    m_ip_address = 0;
    m_last_stat_request_time = 0;

    //configure self
    m_net_server.set_threads_prefix("P2P");
    m_net_server.get_config_object().m_pcommands_handler = this;
    m_net_server.get_config_object().m_invoke_timeout = P2P_DEFAULT_INVOKE_TIMEOUT;
    m_net_server.set_connection_filter(this);

    // from here onwards, it's online stuff
    if (m_offline)
      return res;

    //try to bind
    MINFO("Binding on " << m_bind_ip << ":" << m_port);
    res = m_net_server.init_server(m_port, m_bind_ip);
    CHECK_AND_ASSERT_MES(res, false, "Failed to bind server");

    m_listenning_port = m_net_server.get_binded_port();
    MLOG_GREEN(el::Level::Info, "Net service bound to " << m_bind_ip << ":" << m_listenning_port);
    if(m_external_port)
      MDEBUG("External port defined as " << m_external_port);

    // Add UPnP port mapping
    if(m_no_igd == false) {
      MDEBUG("Attempting to add IGD port mapping.");
      int result;
#if MINIUPNPC_API_VERSION > 13
      // default according to miniupnpc.h
      unsigned char ttl = 2;
      UPNPDev* deviceList = upnpDiscover(1000, NULL, NULL, 0, 0, ttl, &result);
#else
      UPNPDev* deviceList = upnpDiscover(1000, NULL, NULL, 0, 0, &result);
#endif
      UPNPUrls urls;
      IGDdatas igdData;
      char lanAddress[64];
      result = UPNP_GetValidIGD(deviceList, &urls, &igdData, lanAddress, sizeof lanAddress);
      freeUPNPDevlist(deviceList);
      if (result != 0) {
        if (result == 1) {
          std::ostringstream portString;
          portString << m_listenning_port;

          // Delete the port mapping before we create it, just in case we have dangling port mapping from the daemon not being shut down correctly
          UPNP_DeletePortMapping(urls.controlURL, igdData.first.servicetype, portString.str().c_str(), "TCP", 0);

          int portMappingResult;
          portMappingResult = UPNP_AddPortMapping(urls.controlURL, igdData.first.servicetype, portString.str().c_str(), portString.str().c_str(), lanAddress, CRYPTONOTE_NAME, "TCP", 0, "0");
          if (portMappingResult != 0) {
            LOG_ERROR("UPNP_AddPortMapping failed, error: " << strupnperror(portMappingResult));
          } else {
            MLOG_GREEN(el::Level::Info, "Added IGD port mapping.");
          }
        } else if (result == 2) {
          MWARNING("IGD was found but reported as not connected.");
        } else if (result == 3) {
          MWARNING("UPnP device was found but not recognized as IGD.");
        } else {
          MWARNING("UPNP_GetValidIGD returned an unknown result code.");
        }

        FreeUPNPUrls(&urls);
      } else {
        MINFO("No IGD was found.");
      }
    }
    return res;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  typename node_server<t_payload_net_handler>::payload_net_handler& node_server<t_payload_net_handler>::get_payload_object()
  {
    return m_payload_handler;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::run()
  {
    // creating thread to log number of connections
    mPeersLoggerThread.reset(new boost::thread([&]()
    {
      _note("Thread monitor number of peers - start");
      while (!is_closing && !m_net_server.is_stop_signal_sent())
      { // main loop of thread
        //number_of_peers = m_net_server.get_config_object().get_connections_count();
        unsigned int number_of_peers = 0;
        m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
        {
          if (!cntxt.m_is_income) ++number_of_peers;
          return true;
        }); // lambda

        m_current_number_of_out_peers = number_of_peers;

        boost::this_thread::sleep_for(boost::chrono::seconds(1));
      } // main loop of thread
      _note("Thread monitor number of peers - done");
    })); // lambda

    //here you can set worker threads count
    int thrds_count = 10;

    m_net_server.add_idle_handler(boost::bind(&node_server<t_payload_net_handler>::idle_worker, this), 1000);
    m_net_server.add_idle_handler(boost::bind(&t_payload_net_handler::on_idle, &m_payload_handler), 1000);

    boost::thread::attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);

    //go to loop
    MINFO("Run net_service loop( " << thrds_count << " threads)...");
    if(!m_net_server.run_server(thrds_count, true, attrs))
    {
      LOG_ERROR("Failed to run net tcp server!");
    }

    MINFO("net_service loop stopped.");
    return true;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  uint64_t node_server<t_payload_net_handler>::get_connections_count()
  {
    return m_net_server.get_config_object().get_connections_count();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::deinit()
  {
    kill();
    m_peerlist.deinit();
    m_net_server.deinit_server();
    return store_config();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::store_config()
  {

    TRY_ENTRY();
    if (!tools::create_directories_if_necessary(m_config_folder))
    {
      MWARNING("Failed to create data directory: " << m_config_folder);
      return false;
    }

    std::string state_file_path = m_config_folder + "/" + P2P_NET_DATA_FILENAME;
    std::ofstream p2p_data;
    p2p_data.open( state_file_path , std::ios_base::binary | std::ios_base::out| std::ios::trunc);
    if(p2p_data.fail())
    {
      MWARNING("Failed to save config to file " << state_file_path);
      return false;
    };

    boost::archive::portable_binary_oarchive a(p2p_data);
    a << *this;
    return true;
    CATCH_ENTRY_L0("blockchain_storage::save", false);

    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::send_stop_signal()
  {
    std::list<boost::uuids::uuid> connection_ids;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt) {
      connection_ids.push_back(cntxt.m_connection_id);
      return true;
    });
    for (const auto &connection_id: connection_ids)
      m_net_server.get_config_object().close(connection_id);

    m_payload_handler.stop();
    m_net_server.send_stop_signal();
    MDEBUG("[node] Stop signal sent");
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::notify_peer_list(int command, const std::string& buf, const std::vector<peerlist_entry>& peers_to_send, bool try_connect)
  {
      LOG_PRINT_L0("P2P Request: notify_peer_list: start notify");
      for (unsigned i = 0; i < peers_to_send.size(); i++) {
          const peerlist_entry &pe = peers_to_send[i];
          p2p_connection_context con = AUTO_VAL_INIT(con);
          bool is_conneted = find_connection_context_by_peer_id(pe.id, con);
          LOG_PRINT_L0("P2P Request: notify_peer_list: notification " << i);
          if (is_conneted) {
              LOG_PRINT_L0("P2P Request: notify_peer_list: peer is connected, sending to : " << pe.adr.host_str());
              relay_notify(command, buf, con);
          } else if (try_connect) {
              LOG_PRINT_L0("P2P Request: notify_peer_list: connect to notify");
              const epee::net_utils::network_address& na = pe.adr;
              const epee::net_utils::ipv4_network_address &ipv4 = na.as<const epee::net_utils::ipv4_network_address>();

              if (m_net_server.connect(epee::string_tools::get_ip_string_from_int32(ipv4.ip()),
                                       epee::string_tools::num_to_string_fast(ipv4.port()),
                                       m_config.m_net_config.connection_timeout, con, m_bind_ip)) {
                  LOG_PRINT_L0("P2P Request: notify_peer_list: connected to peer: " << pe.adr.host_str()
                               << ", sending command");
                  relay_notify(command, buf, con);
              } else {
                  MWARNING("P2P Request: notify_peer_list: failed to connect to peer: " << pe.adr.host_str());
              }
          }
      }
      LOG_PRINT_L0("P2P Request: notify_peer_list: end notify");
      return true;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::multicast_send(int command, const string &data, const std::list<string> &addresses,
                                                          const std::list<peerid_type> &exclude_peerids)
  {
      LOG_PRINT_L0("P2P Request: multicast_send: Start tunneling for addresses: "
                   << boost::algorithm::join(addresses, ", "));
      std::vector<peerlist_entry> tunnels;
      {
          std::map<std::string, nodetool::supernode_route> local_supernode_routes;
          {
              LOG_PRINT_L0("P2P Request: multicast_send: lock");
              boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
              LOG_PRINT_L0("P2P Request: multicast_send: unlock");
              local_supernode_routes = m_supernode_routes;
          }
          for (auto addr : addresses)
          {
              LOG_PRINT_L0("P2P Request: multicast_send: find tunnel for " << addr);
              auto it = local_supernode_routes.find(addr);
              if (it == local_supernode_routes.end())
              {
                  MWARNING("no tunnel found for address: " << addr);
                  continue;
              }
              std::unordered_map<peerid_type, peerlist_entry> addr_tunnels = (*it).second.peers;
              unsigned int count = 0;
              for (auto peer_it = addr_tunnels.begin(); peer_it != addr_tunnels.end(); ++peer_it)
              {
                  peerlist_entry addr_tunnel = (*peer_it).second;
                  auto tunnel_it = std::find_if(tunnels.begin(), tunnels.end(),
                                                [addr_tunnel](const peerlist_entry &entry) -> bool {
                      return entry.id == addr_tunnel.id;
                  });
                  auto exclude_it = std::find(exclude_peerids.begin(), exclude_peerids.end(),
                                              addr_tunnel.id);
                  if (tunnel_it == tunnels.end() && exclude_it == exclude_peerids.end())
                  {
                      tunnels.push_back(addr_tunnel);
                      count++;
                  }
                  if (count >= MAX_TUNNEL_PEERS)
                  {
                      break;
                  }
              }
          }
      }
      LOG_PRINT_L0("P2P Request: multicast_send: End tunneling");
      return notify_peer_list(command, data, tunnels, true);
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  uint64_t node_server<t_payload_net_handler>::get_max_hop(const std::list<std::string> &addresses)
  {
      uint64_t max_hop = 0;
      {
          std::map<std::string, nodetool::supernode_route> local_supernode_routes;
          {
              LOG_PRINT_L0("P2P Request: multicast_send: lock");
              boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
              LOG_PRINT_L0("P2P Request: multicast_send: unlock");
              local_supernode_routes = m_supernode_routes;
          }
          for (auto addr : addresses)
          {
              auto it = local_supernode_routes.find(addr);
              if (it != local_supernode_routes.end() && max_hop < (*it).second.max_hop)
              {
                  max_hop = (*it).second.max_hop;
              }
          }
      }
      return max_hop;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  std::list<std::string> node_server<t_payload_net_handler>::get_routes()
  {
      std::list<std::string> routes;
      {
          std::map<std::string, nodetool::supernode_route> local_supernode_routes;
          {
              LOG_PRINT_L0("P2P Request: multicast_send: lock");
              boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
              LOG_PRINT_L0("P2P Request: multicast_send: unlock");
              local_supernode_routes = m_supernode_routes;
          }
          for (auto it = local_supernode_routes.begin(); it != local_supernode_routes.end(); ++it)
          {
              routes.push_back((*it).first);
          }
      }
      return routes;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::remove_old_request_cache()
  {
      int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      for (auto it = m_supernode_requests_timestamps.begin(); it != m_supernode_requests_timestamps.end();)
      {
          if ((*it).first + REQUEST_CACHE_TIME < timestamp)
          {
              m_supernode_requests_cache.erase((*it).second);
              it = m_supernode_requests_timestamps.erase(it);
          }
          else
          {
              break;
          }
      }
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_supernode_announce(int command, COMMAND_SUPERNODE_ANNOUNCE::request& arg, p2p_connection_context& context)
  {
      LOG_PRINT_L0("P2P Request: handle_supernode_announce: start");
#ifdef LOCK_RTA_SENDING
    return 1;
#endif
      static std::string supernode_endpoint("send_supernode_announce");
      std::string supernode_str = arg.address;

      if (supernode_str == m_supernode_str)
      {
          return 1;
      }

      LOG_PRINT_L0("P2P Request: handle_supernode_announce: update tunnels for " << arg.address << " Hop: " << arg.hop << " Address: " << arg.network_address);
      do {
          peerlist_entry pe;
          // TODO: Need to investigate it and mechanism for adding peer to the peerlist
          if (!m_peerlist.find_peer(context.peer_id, pe))
          { // unknown peer, alternative handshake with it
              LOG_PRINT_L0("unknown peer, alternative handshake with it " << context.peer_id);
              return 1;
          }
          {
              LOG_PRINT_L0("P2P Request: handle_supernode_announce: lock");
              boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
              LOG_PRINT_L0("P2P Request: handle_supernode_announce: unlock");
              remove_old_request_cache();
          }
          LOG_PRINT_L0("P2P Request: handle_supernode_announce: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
          LOG_PRINT_L0("P2P Request: handle_supernode_announce: unlock");

          LOG_PRINT_L0("P2P Request: handle_supernode_announce: routes number - " << m_supernode_routes.size());
          for (auto it2 = m_supernode_routes.begin(); it2 != m_supernode_routes.end(); ++it2)
          {
              LOG_PRINT_L0("P2P Request: handle_supernode_announce: " << (*it2).first << " " << (*it2).second.peers.size());
          }

          auto it = m_supernode_routes.find(supernode_str);
          if (it == m_supernode_routes.end())
          {
              std::unordered_map<peerid_type, peerlist_entry> peer_map;
              peer_map[pe.id] = pe;
              nodetool::supernode_route route;
              route.last_announce_time = arg.timestamp;
              route.max_hop = arg.hop;
              route.peers = peer_map;
              m_supernode_routes[supernode_str] = route;
              break;
          }

          if ((*it).second.last_announce_time > arg.timestamp)
          {
              MINFO("SUPERNODE_ANNOUNCE from " << context.peer_id
                    << " too old, corrent route timestamp " << (*it).second.last_announce_time);
              return 1;
          }

          if ((*it).second.last_announce_time == arg.timestamp)
          {
              auto peer_it = (*it).second.peers.find(pe.id);
              if (peer_it != (*it).second.peers.end())
              {
                  return 1;
              }
              if (peer_it == (*it).second.peers.end())
              {
                  (*it).second.peers[pe.id] = pe;
                  if ((*it).second.max_hop < arg.hop)
                  {
                      (*it).second.max_hop = arg.hop;
                  }
                  return 1;
              }
          }
          (*it).second.peers.clear();
          (*it).second.peers[pe.id] = pe;
          (*it).second.last_announce_time = arg.timestamp;
          (*it).second.max_hop = arg.hop;
      } while(0);

      LOG_PRINT_L0("P2P Request: handle_supernode_announce: post to supernode");
      do {
//          boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
          if (!m_have_supernode)
              break;
          post_request_to_supernode<cryptonote::COMMAND_RPC_SUPERNODE_ANNOUNCE>(supernode_endpoint, arg);
      } while(0);

      // Notify neighbours about new ANNOUNCE
      arg.hop++;
      LOG_PRINT_L0("P2P Request: handle_supernode_announce: notify peers " << arg.hop);
      std::string arg_buff;
      epee::serialization::store_t_to_binary(arg, arg_buff);
      relay_notify_to_all(command, arg_buff, context);
      LOG_PRINT_L0("P2P Request: handle_supernode_announce: end");
      return 1;
  }

  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_broadcast(int command, typename COMMAND_BROADCAST::request &arg, p2p_connection_context &context)
  {
      LOG_PRINT_L0("P2P Request: handle_broadcast: start");
      LOG_PRINT_L0("P2P Request: handle_broadcast: sender_address: " << arg.sender_address
                   << ", our address: " << m_supernode_str);

#ifdef LOCK_RTA_SENDING
    return 1;
#endif

      {
          LOG_PRINT_L0("P2P Request: handle_broadcast: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
          LOG_PRINT_L0("P2P Request: handle_broadcast: unlock");
          if (m_supernode_requests_cache.find(arg.message_id) == m_supernode_requests_cache.end())
          {
              LOG_PRINT_L0("P2P Request: handle_broadcast: post to supernode");
              m_supernode_requests_cache.insert(arg.message_id);
              int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
              m_supernode_requests_timestamps.insert(std::make_pair(timestamp, arg.message_id));
              if (m_have_supernode)
              {
                  post_request_to_supernode<cryptonote::COMMAND_RPC_BROADCAST>("broadcast", arg, arg.callback_uri);
              }
          }
          LOG_PRINT_L0("P2P Request: handle_broadcast: clean request cache");
          remove_old_request_cache();
      }
      int next_hop = arg.hop - 1;
      LOG_PRINT_L0("P2P Request: handle_broadcast: notify peers " << next_hop);
      if (next_hop >= 0)
      {
          arg.hop = next_hop;
          std::string buff;
          epee::serialization::store_t_to_binary(arg, buff);
          relay_notify_to_all(command, buff, context);
      }
      LOG_PRINT_L0("P2P Request: handle_broadcast: end");
      return 1;
  }

  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_multicast(int command, typename COMMAND_MULTICAST::request &arg, p2p_connection_context &context)
  {
      LOG_PRINT_L0("P2P Request: handle_multicast: start");
      LOG_PRINT_L0("P2P Request: handle_multicast: sender_address: " << arg.sender_address
                   << ", receiver_addresses: " << boost::algorithm::join(arg.receiver_addresses, ", ")
                   << ", our address: " << m_supernode_str);

#ifdef LOCK_RTA_SENDING
    return 1;
#endif

      std::list<std::string> addresses = arg.receiver_addresses;
      {
          LOG_PRINT_L0("P2P Request: handle_multicast: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
          LOG_PRINT_L0("P2P Request: handle_multicast: unlock");
          if (m_supernode_requests_cache.find(arg.message_id) == m_supernode_requests_cache.end())
          {
              LOG_PRINT_L0("P2P Request: handle_multicast: post to supernode");
              m_supernode_requests_cache.insert(arg.message_id);
              int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
              m_supernode_requests_timestamps.insert(std::make_pair(timestamp, arg.message_id));
              auto it = std::find(addresses.begin(), addresses.end(), m_supernode_str);
              if (m_have_supernode && it != addresses.end())
              {
                  LOG_PRINT_L0("P2P Request: handle_multicast: sending to supernode...");
                  post_request_to_supernode<cryptonote::COMMAND_RPC_MULTICAST>("multicast", arg, arg.callback_uri);
              }
          }
          else
          {
            MWARNING("P2P Request: handle_multicast: request found in cache, skipping");
          }
          LOG_PRINT_L0("P2P Request: handle_multicast: clean request cache");
          remove_old_request_cache();
      }
      int next_hop = arg.hop - 1;
      LOG_PRINT_L0("P2P Request: handle_multicast: notify receivers " << next_hop);
      if (next_hop >= 0)
      {
          arg.hop = next_hop;
          std::list<peerid_type> exclude_peers;
          exclude_peers.push_back(context.peer_id);

          std::string buff;
          epee::serialization::store_t_to_binary(arg, buff);
          addresses.remove(m_supernode_str);
          multicast_send(command, buff, addresses, exclude_peers);
      }
      LOG_PRINT_L0("P2P Request: handle_multicast: end");
      return 1;
  }

  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_unicast(int command, typename COMMAND_UNICAST::request &arg, p2p_connection_context &context)
  {
      LOG_PRINT_L0("P2P Request: handle_unicast: start");
      LOG_PRINT_L0("P2P Request: handle_unicast: sender_address: " << arg.sender_address
                   << ", receiver_address: " << arg.receiver_address
                   << ", our address: " << m_supernode_str);

#ifdef LOCK_RTA_SENDING
    return 1;
#endif

      std::string address = arg.receiver_address;
      {
          LOG_PRINT_L0("P2P Request: handle_unicast: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
          LOG_PRINT_L0("P2P Request: handle_unicast: unlock");
          if (m_supernode_requests_cache.find(arg.message_id) == m_supernode_requests_cache.end())
          {
              LOG_PRINT_L0("P2P Request: handle_unicast: post to supernode");
              m_supernode_requests_cache.insert(arg.message_id);
              int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
              m_supernode_requests_timestamps.insert(std::make_pair(timestamp, arg.message_id));
              if (m_have_supernode && address == m_supernode_str)
              {
                  LOG_PRINT_L0("P2P Request: handle_unicast: sending to supernode...");
                  post_request_to_supernode<cryptonote::COMMAND_RPC_UNICAST>("unicast", arg, arg.callback_uri);
              }
          }
          else
          {
              MWARNING("P2P Request: handle_unicast: request found in cache, skipping");
          }
          LOG_PRINT_L0("P2P Request: handle_unicast: clean request cache");
          remove_old_request_cache();
      }
      int next_hop = arg.hop - 1;
      LOG_PRINT_L0("P2P Request: handle_unicast: notidy receiver " << next_hop);
      if (address != m_supernode_str && next_hop >= 0)
      {
          arg.hop = next_hop;
          std::list<std::string> addresses;
          addresses.push_back(address);

          std::list<peerid_type> exclude_peers;
          exclude_peers.push_back(context.peer_id);

          std::string buff;
          epee::serialization::store_t_to_binary(arg, buff);
          multicast_send(command, buff, addresses, exclude_peers);
      }
      LOG_PRINT_L0("P2P Request: handle_unicast: end");
      return 1;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::do_handshake_with_peer(peerid_type& pi, p2p_connection_context& context_, bool just_take_peerlist)
  {
    typename COMMAND_HANDSHAKE::request arg;
    typename COMMAND_HANDSHAKE::response rsp;
    get_local_node_data(arg.node_data);
    m_payload_handler.get_payload_sync_data(arg.payload_data);

    epee::simple_event ev;
    std::atomic<bool> hsh_result(false);
    std::chrono::high_resolution_clock::time_point t1 (std::chrono::high_resolution_clock::now());

    bool r = epee::net_utils::async_invoke_remote_command2<typename COMMAND_HANDSHAKE::response>(context_.m_connection_id, COMMAND_HANDSHAKE::ID, arg, m_net_server.get_config_object(),
      [this, &pi, &ev, &hsh_result, &just_take_peerlist](int code, const typename COMMAND_HANDSHAKE::response& rsp, p2p_connection_context& context)
    {
      epee::misc_utils::auto_scope_leave_caller scope_exit_handler = epee::misc_utils::create_scope_leave_handler([&](){ev.raise();});

      if(code < 0)
      {
        LOG_WARNING_CC(context, "COMMAND_HANDSHAKE invoke failed. (" << code <<  ", " << epee::levin::get_err_descr(code) << ")");
        return;
      }

      if(rsp.node_data.network_id != m_network_id)
      {
        LOG_WARNING_CC(context, "COMMAND_HANDSHAKE Failed, wrong network!  (" << epee::string_tools::get_str_from_guid_a(rsp.node_data.network_id) << "), closing connection.");
        return;
      }

      if(!handle_remote_peerlist(rsp.local_peerlist_new, rsp.node_data.local_time, context))
      {
        LOG_WARNING_CC(context, "COMMAND_HANDSHAKE: failed to handle_remote_peerlist(...), closing connection.");
        add_host_fail(context.m_remote_address);
        return;
      }
      hsh_result = true;
      if(!just_take_peerlist)
      {
        if(!m_payload_handler.process_payload_sync_data(rsp.payload_data, context, true))
        {
          LOG_WARNING_CC(context, "COMMAND_HANDSHAKE invoked, but process_payload_sync_data returned false, dropping connection.");
          hsh_result = false;
          return;
        }

        pi = context.peer_id = rsp.node_data.peer_id;
        m_peerlist.set_peer_just_seen(rsp.node_data.peer_id, context.m_remote_address);

        if(rsp.node_data.peer_id == m_config.m_peer_id)
        {
          LOG_DEBUG_CC(context, "Connection to self detected, dropping connection");
          hsh_result = false;
          return;
        }
        LOG_DEBUG_CC(context, " COMMAND_HANDSHAKE INVOKED OK");
      }else
      {
        LOG_DEBUG_CC(context, " COMMAND_HANDSHAKE(AND CLOSE) INVOKED OK");
      }
    }, P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT);

    std::chrono::high_resolution_clock::time_point t2 (std::chrono::high_resolution_clock::now());


    if(r)
    {
      ev.wait();
    }

    if(!hsh_result)
    {
      LOG_WARNING_CC(context_, "COMMAND_HANDSHAKE Failed");
      m_net_server.get_config_object().close(context_.m_connection_id);
    }
    else
    {
      try_get_support_flags(context_, [](p2p_connection_context& flags_context, const uint32_t& support_flags)
      {
        flags_context.support_flags = support_flags;
      });
    }

    return hsh_result;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::do_peer_timed_sync(const epee::net_utils::connection_context_base& context_, peerid_type peer_id)
  {
    typename COMMAND_TIMED_SYNC::request arg = AUTO_VAL_INIT(arg);
    m_payload_handler.get_payload_sync_data(arg.payload_data);

    bool r = epee::net_utils::async_invoke_remote_command2<typename COMMAND_TIMED_SYNC::response>(context_.m_connection_id, COMMAND_TIMED_SYNC::ID, arg, m_net_server.get_config_object(),
      [this](int code, const typename COMMAND_TIMED_SYNC::response& rsp, p2p_connection_context& context)
    {
      context.m_in_timedsync = false;
      if(code < 0)
      {
        LOG_WARNING_CC(context, "COMMAND_TIMED_SYNC invoke failed. (" << code <<  ", " << epee::levin::get_err_descr(code) << ")");
        return;
      }

      if(!handle_remote_peerlist(rsp.local_peerlist_new, rsp.local_time, context))
      {
        LOG_WARNING_CC(context, "COMMAND_TIMED_SYNC: failed to handle_remote_peerlist(...), closing connection.");
        m_net_server.get_config_object().close(context.m_connection_id );
        add_host_fail(context.m_remote_address);
      }
      if(!context.m_is_income)
        m_peerlist.set_peer_just_seen(context.peer_id, context.m_remote_address);
      m_payload_handler.process_payload_sync_data(rsp.payload_data, context, false);
    });

    if(!r)
    {
      LOG_WARNING_CC(context_, "COMMAND_TIMED_SYNC Failed");
      return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_random_index_with_fixed_probability(size_t max_index)
  {
    //divide by zero workaround
    if(!max_index)
      return 0;

    size_t x = crypto::rand<size_t>()%(max_index+1);
    size_t res = (x*x*x)/(max_index*max_index); //parabola \/
    MDEBUG("Random connection index=" << res << "(x="<< x << ", max_index=" << max_index << ")");
    return res;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_peer_used(const peerlist_entry& peer)
  {

    if(m_config.m_peer_id == peer.id)
      return true;//dont make connections to ourself

    bool used = false;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(cntxt.peer_id == peer.id || (!cntxt.m_is_income && peer.adr == cntxt.m_remote_address))
      {
        used = true;
        return false;//stop enumerating
      }
      return true;
    });

    return used;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_peer_used(const anchor_peerlist_entry& peer)
  {
    if(m_config.m_peer_id == peer.id) {
        return true;//dont make connections to ourself
    }

    bool used = false;

    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(cntxt.peer_id == peer.id || (!cntxt.m_is_income && peer.adr == cntxt.m_remote_address))
      {
        used = true;

        return false;//stop enumerating
      }

      return true;
    });

    return used;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_addr_connected(const epee::net_utils::network_address& peer)
  {
    bool connected = false;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(!cntxt.m_is_income && peer == cntxt.m_remote_address)
      {
        connected = true;
        return false;//stop enumerating
      }
      return true;
    });

    return connected;
  }

#define LOG_PRINT_CC_PRIORITY_NODE(priority, con, msg) \
  do { \
    if (priority) {\
      LOG_INFO_CC(con, "[priority]" << msg); \
    } else {\
      LOG_INFO_CC(con, msg); \
    } \
  } while(0)

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::try_to_connect_and_handshake_with_new_peer(const epee::net_utils::network_address& na, bool just_take_peerlist, uint64_t last_seen_stamp, PeerType peer_type, uint64_t first_seen_stamp)
  {
    if (m_current_number_of_out_peers == m_config.m_net_config.connections_count) // out peers limit
    {
      return false;
    }
    else if (m_current_number_of_out_peers > m_config.m_net_config.connections_count)
    {
      m_net_server.get_config_object().del_out_connections(1);
      m_current_number_of_out_peers --; // atomic variable, update time = 1s
      return false;
    }
    MDEBUG("Connecting to " << na.str() << "(peer_type=" << peer_type << ", last_seen: "
        << (last_seen_stamp ? epee::misc_utils::get_time_interval_string(time(NULL) - last_seen_stamp):"never")
        << ")...");

    CHECK_AND_ASSERT_MES(na.get_type_id() == epee::net_utils::ipv4_network_address::ID, false,
        "Only IPv4 addresses are supported here");
    const epee::net_utils::ipv4_network_address &ipv4 = na.as<const epee::net_utils::ipv4_network_address>();

    typename net_server::t_connection_context con = AUTO_VAL_INIT(con);
    bool res = m_net_server.connect(epee::string_tools::get_ip_string_from_int32(ipv4.ip()),
      epee::string_tools::num_to_string_fast(ipv4.port()),
      m_config.m_net_config.connection_timeout,
      con, m_bind_ip);

    if(!res)
    {
      bool is_priority = is_priority_node(na);
      LOG_PRINT_CC_PRIORITY_NODE(is_priority, con, "Connect failed to " << na.str()
        /*<< ", try " << try_count*/);
      //m_peerlist.set_peer_unreachable(pe);
      return false;
    }

    peerid_type pi = AUTO_VAL_INIT(pi);
    res = do_handshake_with_peer(pi, con, just_take_peerlist);

    if(!res)
    {
      bool is_priority = is_priority_node(na);
      LOG_PRINT_CC_PRIORITY_NODE(is_priority, con, "Failed to HANDSHAKE with peer "
        << na.str()
        /*<< ", try " << try_count*/);
      return false;
    }

    if(just_take_peerlist)
    {
      m_net_server.get_config_object().close(con.m_connection_id);
      LOG_DEBUG_CC(con, "CONNECTION HANDSHAKED OK AND CLOSED.");
      return true;
    }

    peerlist_entry pe_local = AUTO_VAL_INIT(pe_local);
    pe_local.adr = na;
    pe_local.id = pi;
    time_t last_seen;
    time(&last_seen);
    pe_local.last_seen = static_cast<int64_t>(last_seen);
    m_peerlist.append_with_peer_white(pe_local);
    //update last seen and push it to peerlist manager

    anchor_peerlist_entry ape = AUTO_VAL_INIT(ape);
    ape.adr = na;
    ape.id = pi;
    ape.first_seen = first_seen_stamp ? first_seen_stamp : time(nullptr);

    m_peerlist.append_with_peer_anchor(ape);

    LOG_DEBUG_CC(con, "CONNECTION HANDSHAKED OK.");
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::check_connection_and_handshake_with_peer(const epee::net_utils::network_address& na, uint64_t last_seen_stamp)
  {
    LOG_PRINT_L1("Connecting to " << na.str() << "(last_seen: "
                                  << (last_seen_stamp ? epee::misc_utils::get_time_interval_string(time(NULL) - last_seen_stamp):"never")
                                  << ")...");

    CHECK_AND_ASSERT_MES(na.get_type_id() == epee::net_utils::ipv4_network_address::ID, false,
        "Only IPv4 addresses are supported here");
    const epee::net_utils::ipv4_network_address &ipv4 = na.as<epee::net_utils::ipv4_network_address>();

    typename net_server::t_connection_context con = AUTO_VAL_INIT(con);
    bool res = m_net_server.connect(epee::string_tools::get_ip_string_from_int32(ipv4.ip()),
                                    epee::string_tools::num_to_string_fast(ipv4.port()),
                                    m_config.m_net_config.connection_timeout,
                                    con, m_bind_ip);

    if (!res) {
      bool is_priority = is_priority_node(na);

      LOG_PRINT_CC_PRIORITY_NODE(is_priority, con, "Connect failed to " << na.str());

      return false;
    }

    peerid_type pi = AUTO_VAL_INIT(pi);
    res = do_handshake_with_peer(pi, con, true);

    if (!res) {
      bool is_priority = is_priority_node(na);

      LOG_PRINT_CC_PRIORITY_NODE(is_priority, con, "Failed to HANDSHAKE with peer " << na.str());

      return false;
    }

    m_net_server.get_config_object().close(con.m_connection_id);

    LOG_DEBUG_CC(con, "CONNECTION HANDSHAKED OK AND CLOSED.");

    return true;
  }

#undef LOG_PRINT_CC_PRIORITY_NODE

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_addr_recently_failed(const epee::net_utils::network_address& addr)
  {
    CRITICAL_REGION_LOCAL(m_conn_fails_cache_lock);
    auto it = m_conn_fails_cache.find(addr);
    if(it == m_conn_fails_cache.end())
      return false;

    if(time(NULL) - it->second > P2P_FAILED_ADDR_FORGET_SECONDS)
      return false;
    else
      return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_new_connection_from_anchor_peerlist(const std::vector<anchor_peerlist_entry>& anchor_peerlist)
  {
    for (const auto& pe: anchor_peerlist) {
      _note("Considering connecting (out) to peer: " << peerid_type(pe.id) << " " << pe.adr.str());

      if(is_peer_used(pe)) {
        _note("Peer is used");
        continue;
      }

      if(!is_remote_host_allowed(pe.adr)) {
        continue;
      }

      if(is_addr_recently_failed(pe.adr)) {
        continue;
      }

      MDEBUG("Selected peer: " << peerid_to_string(pe.id) << " " << pe.adr.str()
                               << "[peer_type=" << anchor
                               << "] first_seen: " << epee::misc_utils::get_time_interval_string(time(NULL) - pe.first_seen));

      if(!try_to_connect_and_handshake_with_new_peer(pe.adr, false, 0, anchor, pe.first_seen)) {
        _note("Handshake failed");
        continue;
      }

      return true;
    }

    return false;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_new_connection_from_peerlist(bool use_white_list)
  {
    size_t local_peers_count = use_white_list ? m_peerlist.get_white_peers_count():m_peerlist.get_gray_peers_count();
    if(!local_peers_count)
      return false;//no peers

    size_t max_random_index = std::min<uint64_t>(local_peers_count -1, 20);

    std::set<size_t> tried_peers;

    size_t try_count = 0;
    size_t rand_count = 0;
    while(rand_count < (max_random_index+1)*3 &&  try_count < 10 && !m_net_server.is_stop_signal_sent())
    {
      ++rand_count;
      size_t random_index;

      if (use_white_list) {
        local_peers_count = m_peerlist.get_white_peers_count();
        max_random_index = std::min<uint64_t>(local_peers_count -1, 20);
        random_index = get_random_index_with_fixed_probability(max_random_index);
      } else {
        random_index = crypto::rand<size_t>() % m_peerlist.get_gray_peers_count();
      }

      CHECK_AND_ASSERT_MES(random_index < local_peers_count, false, "random_starter_index < peers_local.size() failed!!");

      if(tried_peers.count(random_index))
        continue;

      tried_peers.insert(random_index);
      peerlist_entry pe = AUTO_VAL_INIT(pe);
      bool r = use_white_list ? m_peerlist.get_white_peer_by_index(pe, random_index):m_peerlist.get_gray_peer_by_index(pe, random_index);
      CHECK_AND_ASSERT_MES(r, false, "Failed to get random peer from peerlist(white:" << use_white_list << ")");

      ++try_count;

      _note("Considering connecting (out) to peer: " << peerid_to_string(pe.id) << " " << pe.adr.str());

      if(is_peer_used(pe)) {
        _note("Peer is used");
        continue;
      }

      if(!is_remote_host_allowed(pe.adr))
        continue;

      if(is_addr_recently_failed(pe.adr))
        continue;

      MDEBUG("Selected peer: " << peerid_to_string(pe.id) << " " << pe.adr.str()
                    << "[peer_list=" << (use_white_list ? white : gray)
                    << "] last_seen: " << (pe.last_seen ? epee::misc_utils::get_time_interval_string(time(NULL) - pe.last_seen) : "never"));

      if(!try_to_connect_and_handshake_with_new_peer(pe.adr, false, pe.last_seen, use_white_list ? white : gray)) {
        _note("Handshake failed");
        continue;
      }

      return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::connect_to_seed()
  {
      if (m_seed_nodes.empty())
        return true;

      size_t try_count = 0;
      size_t current_index = crypto::rand<size_t>()%m_seed_nodes.size();
      bool fallback_nodes_added = false;
      while(true)
      {
        if(m_net_server.is_stop_signal_sent())
          return false;

        if(try_to_connect_and_handshake_with_new_peer(m_seed_nodes[current_index], true))
          break;
        if(++try_count > m_seed_nodes.size())
        {
          if (!fallback_nodes_added)
          {
            MWARNING("Failed to connect to any of seed peers, trying fallback seeds");
            for (const auto &peer: get_seed_nodes(m_testnet))
            {
              MDEBUG("Fallback seed node: " << peer);
              append_net_address(m_seed_nodes, peer);
            }
            fallback_nodes_added = true;
            // continue for another few cycles
          }
          else
          {
            MWARNING("Failed to connect to any of seed peers, continuing without seeds");
            break;
          }
        }
        if(++current_index >= m_seed_nodes.size())
          current_index = 0;
      }
      return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::connections_maker()
  {
    if (!connect_to_peerlist(m_exclusive_peers)) return false;

    if (!m_exclusive_peers.empty()) return true;

    size_t start_conn_count = get_outgoing_connections_count();
    if(!m_peerlist.get_white_peers_count() && m_seed_nodes.size())
    {
      if (!connect_to_seed())
        return false;
    }

    if (!connect_to_peerlist(m_priority_peers)) return false;

    size_t expected_white_connections = (m_config.m_net_config.connections_count*P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT)/100;

    size_t conn_count = get_outgoing_connections_count();
    if(conn_count < m_config.m_net_config.connections_count)
    {
      if(conn_count < expected_white_connections)
      {
        //start from anchor list
        if(!make_expected_connections_count(anchor, P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT))
          return false;
        //then do white list
        if(!make_expected_connections_count(white, expected_white_connections))
          return false;
        //then do grey list
        if(!make_expected_connections_count(gray, m_config.m_net_config.connections_count))
          return false;
      }else
      {
        //start from grey list
        if(!make_expected_connections_count(gray, m_config.m_net_config.connections_count))
          return false;
        //and then do white list
        if(!make_expected_connections_count(white, m_config.m_net_config.connections_count))
          return false;
      }
    }

    if (start_conn_count == get_outgoing_connections_count() && start_conn_count < m_config.m_net_config.connections_count)
    {
      MINFO("Failed to connect to any, trying seeds");
      if (!connect_to_seed())
        return false;
    }

    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_expected_connections_count(PeerType peer_type, size_t expected_connections)
  {
    if (m_offline)
      return true;

    std::vector<anchor_peerlist_entry> apl;

    if (peer_type == anchor) {
      m_peerlist.get_and_empty_anchor_peerlist(apl);
    }

    size_t conn_count = get_outgoing_connections_count();
    //add new connections from white peers
    while(conn_count < expected_connections)
    {
      if(m_net_server.is_stop_signal_sent())
        return false;

      if (peer_type == anchor && !make_new_connection_from_anchor_peerlist(apl)) {
        break;
      }

      if (peer_type == white && !make_new_connection_from_peerlist(true)) {
        break;
      }

      if (peer_type == gray && !make_new_connection_from_peerlist(false)) {
        break;
      }

      conn_count = get_outgoing_connections_count();
    }
    return true;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_outgoing_connections_count()
  {
    size_t count = 0;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(!cntxt.m_is_income)
        ++count;
      return true;
    });

    return count;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::idle_worker()
  {
    m_peer_handshake_idle_maker_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::peer_sync_idle_maker, this));
    m_connections_maker_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::connections_maker, this));
    m_gray_peerlist_housekeeping_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::gray_peerlist_housekeeping, this));
    m_peerlist_store_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::store_config, this));
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::peer_sync_idle_maker()
  {
    MDEBUG("STARTED PEERLIST IDLE HANDSHAKE");
    typedef std::list<std::pair<epee::net_utils::connection_context_base, peerid_type> > local_connects_type;
    local_connects_type cncts;
    m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& cntxt)
    {
      if(cntxt.peer_id && !cntxt.m_in_timedsync)
      {
        cntxt.m_in_timedsync = true;
        cncts.push_back(local_connects_type::value_type(cntxt, cntxt.peer_id));//do idle sync only with handshaked connections
      }
      return true;
    });

    std::for_each(cncts.begin(), cncts.end(), [&](const typename local_connects_type::value_type& vl){do_peer_timed_sync(vl.first, vl.second);});

    MDEBUG("FINISHED PEERLIST IDLE HANDSHAKE");
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::fix_time_delta(std::list<peerlist_entry>& local_peerlist, time_t local_time, int64_t& delta)
  {
    //fix time delta
    time_t now = 0;
    time(&now);
    delta = now - local_time;

    for(peerlist_entry& be: local_peerlist)
    {
      if(be.last_seen > local_time)
      {
        MWARNING("FOUND FUTURE peerlist for entry " << be.adr.str() << " last_seen: " << be.last_seen << ", local_time(on remote node):" << local_time);
        return false;
      }
      be.last_seen += delta;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::handle_remote_peerlist(const std::list<peerlist_entry>& peerlist, time_t local_time, const epee::net_utils::connection_context_base& context)
  {
    int64_t delta = 0;
    std::list<peerlist_entry> peerlist_ = peerlist;
    if(!fix_time_delta(peerlist_, local_time, delta))
      return false;
    LOG_DEBUG_CC(context, "REMOTE PEERLIST: TIME_DELTA: " << delta << ", remote peerlist size=" << peerlist_.size());
    LOG_DEBUG_CC(context, "REMOTE PEERLIST: " <<  print_peerlist_to_string(peerlist_));
    return m_peerlist.merge_peerlist(peerlist_);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::get_local_node_data(basic_node_data& node_data)
  {
    time_t local_time;
    time(&local_time);
    node_data.local_time = local_time;
    node_data.peer_id = m_config.m_peer_id;
    if(!m_hide_my_port)
      node_data.my_port = m_external_port ? m_external_port : m_listenning_port;
    else
      node_data.my_port = 0;
    node_data.network_id = m_network_id;
    return true;
  }
  //-----------------------------------------------------------------------------------
#ifdef ALLOW_DEBUG_COMMANDS
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::check_trust(const proof_of_trust& tr)
  {
    uint64_t local_time = time(NULL);
    uint64_t time_delata = local_time > tr.time ? local_time - tr.time: tr.time - local_time;
    if(time_delata > 24*60*60 )
    {
      MWARNING("check_trust failed to check time conditions, local_time=" <<  local_time << ", proof_time=" << tr.time);
      return false;
    }
    if(m_last_stat_request_time >= tr.time )
    {
      MWARNING("check_trust failed to check time conditions, last_stat_request_time=" <<  m_last_stat_request_time << ", proof_time=" << tr.time);
      return false;
    }
    if(m_config.m_peer_id != tr.peer_id)
    {
      MWARNING("check_trust failed: peer_id mismatch (passed " << tr.peer_id << ", expected " << m_config.m_peer_id<< ")");
      return false;
    }
    crypto::public_key pk = AUTO_VAL_INIT(pk);
    epee::string_tools::hex_to_pod(::config::P2P_REMOTE_DEBUG_TRUSTED_PUB_KEY, pk);
    crypto::hash h = get_proof_of_trust_hash(tr);
    if(!crypto::check_signature(h, pk, tr.sign))
    {
      MWARNING("check_trust failed: sign check failed");
      return false;
    }
    //update last request time
    m_last_stat_request_time = tr.time;
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_stat_info(int command, typename COMMAND_REQUEST_STAT_INFO::request& arg, typename COMMAND_REQUEST_STAT_INFO::response& rsp, p2p_connection_context& context)
  {
    if(!check_trust(arg.tr))
    {
      drop_connection(context);
      return 1;
    }
    rsp.connections_count = m_net_server.get_config_object().get_connections_count();
    rsp.incoming_connections_count = rsp.connections_count - get_outgoing_connections_count();
    rsp.version = GRAFT_VERSION_FULL;
    rsp.os_version = tools::get_os_version_string();
    m_payload_handler.get_stat_info(rsp.payload_info);
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_network_state(int command, COMMAND_REQUEST_NETWORK_STATE::request& arg, COMMAND_REQUEST_NETWORK_STATE::response& rsp, p2p_connection_context& context)
  {
    if(!check_trust(arg.tr))
    {
      drop_connection(context);
      return 1;
    }
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      connection_entry ce;
      ce.adr  = cntxt.m_remote_address;
      ce.id = cntxt.peer_id;
      ce.is_income = cntxt.m_is_income;
      rsp.connections_list.push_back(ce);
      return true;
    });

    m_peerlist.get_peerlist_full(rsp.local_peerlist_gray, rsp.local_peerlist_white);
    rsp.my_id = m_config.m_peer_id;
    rsp.local_time = time(NULL);
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_peer_id(int command, COMMAND_REQUEST_PEER_ID::request& arg, COMMAND_REQUEST_PEER_ID::response& rsp, p2p_connection_context& context)
  {
    rsp.my_id = m_config.m_peer_id;
    return 1;
  }
#endif
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_support_flags(int command, COMMAND_REQUEST_SUPPORT_FLAGS::request& arg, COMMAND_REQUEST_SUPPORT_FLAGS::response& rsp, p2p_connection_context& context)
  {
    rsp.support_flags = m_config.m_support_flags;
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::request_callback(const epee::net_utils::connection_context_base& context)
  {
    m_net_server.get_config_object().request_callback(context.m_connection_id);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::relay_notify(int command, const std::string& data_buff, const p2p_connection_context& connection)
  {
      return m_net_server.get_config_object().notify(command, data_buff, connection.m_connection_id) >= 0;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::relay_notify_to_list(int command, const std::string& data_buff, const std::list<boost::uuids::uuid> &connections)
  {
    for(const auto& c_id: connections)
    {
      m_net_server.get_config_object().notify(command, data_buff, c_id);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::relay_notify_to_all(int command, const std::string& data_buff, const epee::net_utils::connection_context_base& context)
  {
    std::list<boost::uuids::uuid> connections;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(cntxt.peer_id && context.m_connection_id != cntxt.m_connection_id)
        connections.push_back(cntxt.m_connection_id);
      return true;
    });
    return relay_notify_to_list(command, data_buff, connections);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::callback(p2p_connection_context& context)
  {
    m_payload_handler.on_callback(context);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::invoke_notify_to_peer(int command, const std::string& req_buff, const epee::net_utils::connection_context_base& context)
  {
    int res = m_net_server.get_config_object().notify(command, req_buff, context.m_connection_id);
    return res > 0;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::invoke_command_to_peer(int command, const std::string& req_buff, std::string& resp_buff, const epee::net_utils::connection_context_base& context)
  {
    int res = m_net_server.get_config_object().invoke(command, req_buff, resp_buff, context.m_connection_id);
    return res > 0;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::drop_connection(const epee::net_utils::connection_context_base& context)
  {
    m_net_server.get_config_object().close(context.m_connection_id);
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler> template<class t_callback>
  bool node_server<t_payload_net_handler>::try_ping(basic_node_data& node_data, p2p_connection_context& context, t_callback cb)
  {
    if(!node_data.my_port)
      return false;

    CHECK_AND_ASSERT_MES(context.m_remote_address.get_type_id() == epee::net_utils::ipv4_network_address::ID, false,
        "Only IPv4 addresses are supported here");

    const epee::net_utils::network_address na = context.m_remote_address;
    uint32_t actual_ip = na.as<const epee::net_utils::ipv4_network_address>().ip();
    if(!m_peerlist.is_host_allowed(context.m_remote_address))
      return false;
    std::string ip = epee::string_tools::get_ip_string_from_int32(actual_ip);
    std::string port = epee::string_tools::num_to_string_fast(node_data.my_port);
    epee::net_utils::network_address address(new epee::net_utils::ipv4_network_address(actual_ip, node_data.my_port));
    peerid_type pr = node_data.peer_id;
    bool r = m_net_server.connect_async(ip, port, m_config.m_net_config.ping_connection_timeout, [cb, /*context,*/ address, pr, this](
      const typename net_server::t_connection_context& ping_context,
      const boost::system::error_code& ec)->bool
    {
      if(ec)
      {
        LOG_WARNING_CC(ping_context, "back ping connect failed to " << address.str());
        return false;
      }
      COMMAND_PING::request req;
      COMMAND_PING::response rsp;
      //vc2010 workaround
      /*std::string ip_ = ip;
      std::string port_=port;
      peerid_type pr_ = pr;
      auto cb_ = cb;*/

      // GCC 5.1.0 gives error with second use of uint64_t (peerid_type) variable.
      peerid_type pr_ = pr;

      bool inv_call_res = epee::net_utils::async_invoke_remote_command2<COMMAND_PING::response>(ping_context.m_connection_id, COMMAND_PING::ID, req, m_net_server.get_config_object(),
        [=](int code, const COMMAND_PING::response& rsp, p2p_connection_context& context)
      {
        if(code <= 0)
        {
          LOG_WARNING_CC(ping_context, "Failed to invoke COMMAND_PING to " << address.str() << "(" << code <<  ", " << epee::levin::get_err_descr(code) << ")");
          return;
        }

        if(rsp.status != PING_OK_RESPONSE_STATUS_TEXT || pr != rsp.peer_id)
        {
          LOG_WARNING_CC(ping_context, "back ping invoke wrong response \"" << rsp.status << "\" from" << address.str() << ", hsh_peer_id=" << pr_ << ", rsp.peer_id=" << rsp.peer_id);
          m_net_server.get_config_object().close(ping_context.m_connection_id);
          return;
        }
        m_net_server.get_config_object().close(ping_context.m_connection_id);
        cb();
      });

      if(!inv_call_res)
      {
        LOG_WARNING_CC(ping_context, "back ping invoke failed to " << address.str());
        m_net_server.get_config_object().close(ping_context.m_connection_id);
        return false;
      }
      return true;
    }, m_bind_ip);
    if(!r)
    {
      LOG_WARNING_CC(context, "Failed to call connect_async, network error.");
    }
    return r;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::try_get_support_flags(const p2p_connection_context& context, std::function<void(p2p_connection_context&, const uint32_t&)> f)
  {
    COMMAND_REQUEST_SUPPORT_FLAGS::request support_flags_request;
    bool r = epee::net_utils::async_invoke_remote_command2<typename COMMAND_REQUEST_SUPPORT_FLAGS::response>
    (
      context.m_connection_id,
      COMMAND_REQUEST_SUPPORT_FLAGS::ID,
      support_flags_request,
      m_net_server.get_config_object(),
      [=](int code, const typename COMMAND_REQUEST_SUPPORT_FLAGS::response& rsp, p2p_connection_context& context_)
      {
        if(code < 0)
        {
          LOG_WARNING_CC(context_, "COMMAND_REQUEST_SUPPORT_FLAGS invoke failed. (" << code <<  ", " << epee::levin::get_err_descr(code) << ")");
          return;
        }

        f(context_, rsp.support_flags);
      },
      P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT
    );

    return r;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_timed_sync(int command, typename COMMAND_TIMED_SYNC::request& arg, typename COMMAND_TIMED_SYNC::response& rsp, p2p_connection_context& context)
  {
    if(!m_payload_handler.process_payload_sync_data(arg.payload_data, context, false))
    {
      LOG_WARNING_CC(context, "Failed to process_payload_sync_data(), dropping connection");
      drop_connection(context);
      return 1;
    }

    //fill response
    rsp.local_time = time(NULL);
    m_peerlist.get_peerlist_head(rsp.local_peerlist_new);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);
    LOG_DEBUG_CC(context, "COMMAND_TIMED_SYNC");
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_handshake(int command, typename COMMAND_HANDSHAKE::request& arg, typename COMMAND_HANDSHAKE::response& rsp, p2p_connection_context& context)
  {
    if(arg.node_data.network_id != m_network_id)
    {

      LOG_INFO_CC(context, "WRONG NETWORK AGENT CONNECTED! id=" << epee::string_tools::get_str_from_guid_a(arg.node_data.network_id));
      drop_connection(context);
      add_host_fail(context.m_remote_address);
      return 1;
    }

    if(!context.m_is_income)
    {
      LOG_WARNING_CC(context, "COMMAND_HANDSHAKE came not from incoming connection");
      drop_connection(context);
      add_host_fail(context.m_remote_address);
      return 1;
    }

    if(context.peer_id)
    {
      LOG_WARNING_CC(context, "COMMAND_HANDSHAKE came, but seems that connection already have associated peer_id (double COMMAND_HANDSHAKE?)");
      drop_connection(context);
      return 1;
    }

    if(!m_payload_handler.process_payload_sync_data(arg.payload_data, context, true))
    {
      LOG_WARNING_CC(context, "COMMAND_HANDSHAKE came, but process_payload_sync_data returned false, dropping connection.");
      drop_connection(context);
      return 1;
    }

    if(has_too_many_connections(context.m_remote_address))
    {
      LOG_PRINT_CCONTEXT_L1("CONNECTION FROM " << context.m_remote_address.host_str() << " REFUSED, too many connections from the same address");
      drop_connection(context);
      return 1;
    }

    //associate peer_id with this connection
    context.peer_id = arg.node_data.peer_id;
    context.m_in_timedsync = false;

    if(arg.node_data.peer_id != m_config.m_peer_id && arg.node_data.my_port)
    {
      peerid_type peer_id_l = arg.node_data.peer_id;
      uint32_t port_l = arg.node_data.my_port;
      //try ping to be sure that we can add this peer to peer_list
      try_ping(arg.node_data, context, [peer_id_l, port_l, context, this]()
      {
        CHECK_AND_ASSERT_MES(context.m_remote_address.get_type_id() == epee::net_utils::ipv4_network_address::ID, void(),
            "Only IPv4 addresses are supported here");
        //called only(!) if success pinged, update local peerlist
        peerlist_entry pe;
        const epee::net_utils::network_address na = context.m_remote_address;
        pe.adr.reset(new epee::net_utils::ipv4_network_address(na.as<epee::net_utils::ipv4_network_address>().ip(), port_l));
        time_t last_seen;
        time(&last_seen);
        pe.last_seen = static_cast<int64_t>(last_seen);
        pe.id = peer_id_l;
        this->m_peerlist.append_with_peer_white(pe);
        LOG_DEBUG_CC(context, "PING SUCCESS " << context.m_remote_address.host_str() << ":" << port_l);
      });
    }
#if 0 // unsupported in production

    try_get_support_flags(context, [](p2p_connection_context& flags_context, const uint32_t& support_flags)
    {
      flags_context.support_flags = support_flags;
    });
#endif
    //fill response
    m_peerlist.get_peerlist_head(rsp.local_peerlist_new);
    get_local_node_data(rsp.node_data);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);
    LOG_DEBUG_CC(context, "COMMAND_HANDSHAKE");
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_ping(int command, COMMAND_PING::request& arg, COMMAND_PING::response& rsp, p2p_connection_context& context)
  {
    LOG_DEBUG_CC(context, "COMMAND_PING");
    rsp.status = PING_OK_RESPONSE_STATUS_TEXT;
    rsp.peer_id = m_config.m_peer_id;
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::log_peerlist()
  {
    std::list<peerlist_entry> pl_white;
    std::list<peerlist_entry> pl_gray;
    m_peerlist.get_peerlist_full(pl_gray, pl_white);
    MINFO(ENDL << "Peerlist white:" << ENDL << print_peerlist_to_string(pl_white) << ENDL << "Peerlist gray:" << ENDL << print_peerlist_to_string(pl_gray) );
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::log_connections()
  {
    MINFO("Connections: \r\n" << print_connections_container() );
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::do_supernode_announce(const cryptonote::COMMAND_RPC_SUPERNODE_ANNOUNCE::request &req)
  {
    LOG_PRINT_L0("Incoming supernode announce request");
#ifdef LOCK_RTA_SENDING
    return;
#endif

    LOG_PRINT_L0("P2P Request: do_supernode_announce: start");

    COMMAND_SUPERNODE_ANNOUNCE::request p2p_req;
    p2p_req.signed_key_images = req.signed_key_images;
    p2p_req.timestamp = req.timestamp;
    p2p_req.address = req.address;
    p2p_req.stake_amount = req.stake_amount;
    p2p_req.height = req.height;
    p2p_req.secret_viewkey = req.secret_viewkey;
    p2p_req.network_address = req.network_address;
    p2p_req.hop = 0;

    LOG_PRINT_L0("P2P Request: do_supernode_announce: prepare peerlist");
    std::string blob;
    epee::serialization::store_t_to_binary(p2p_req, blob);
    std::set<peerid_type> announced_peers;
    // send to peers
    m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& context) {
        LOG_INFO_CC(context, "invoking COMMAND_SUPERNODE_ANNOUNCE");
        if (invoke_notify_to_peer(COMMAND_SUPERNODE_ANNOUNCE::ID, blob, context)) {
            announced_peers.insert(context.peer_id);
            return true;
        }
        LOG_ERROR_CC(context, "failed to invoke COMMAND_SUPERNODE_ANNOUNCE");
        return false;
    });

    std::list<peerlist_entry> peerlist_white, peerlist_gray;
    m_peerlist.get_peerlist_full(peerlist_white,peerlist_gray);
    std::vector<peerlist_entry> peers_to_send;
    for (auto pe :peerlist_white) {
        if (announced_peers.find(pe.id) != announced_peers.end()) {
            continue;
        }
        peers_to_send.push_back(pe);
    }

    LOG_PRINT_L0("P2P Request: do_supernode_announce: notify_peer_list");
    notify_peer_list(COMMAND_SUPERNODE_ANNOUNCE::ID,blob,peers_to_send);
    LOG_PRINT_L0("P2P Request: do_supernode_announce: end");
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::do_broadcast(const cryptonote::COMMAND_RPC_BROADCAST::request &req)
  {
      LOG_PRINT_L0("Incoming broadcast request");

      LOG_PRINT_L0("P2P Request: do_broadcast from:" << req.sender_address <<
                   " Start");

      std::string data_blob;
      epee::serialization::store_t_to_binary(req, data_blob);
      std::vector<uint8_t> data_vec(data_blob.begin(), data_blob.end());
      crypto::hash message_hash;
      if (!tools::sha256sum(data_vec.data(), data_vec.size(), message_hash))
      {
          LOG_ERROR("RTA Broadcast: wrong data format for hashing!");
          return;
      }

      LOG_PRINT_L0("P2P Request: do_broadcast: broadcast to me");
      {
//          LOG_PRINT_L0("P2P Request: do_broadcast: lock");
//          boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
//          LOG_PRINT_L0("P2P Request: do_broadcast: unlock");
          if (m_have_supernode)
          {
              post_request_to_supernode<cryptonote::COMMAND_RPC_BROADCAST>("broadcast", req, req.callback_uri);
          }
      }

#ifdef LOCK_RTA_SENDING
    return;
#endif

      COMMAND_BROADCAST::request p2p_req = AUTO_VAL_INIT(p2p_req);
      p2p_req.sender_address = req.sender_address;
      p2p_req.callback_uri = req.callback_uri;
      p2p_req.data = req.data;
      p2p_req.wait_answer = req.wait_answer;
      p2p_req.hop = HOP_RETRIES_MULTIPLIER * get_max_hop(get_routes());
      p2p_req.message_id = epee::string_tools::pod_to_hex(message_hash);

      LOG_PRINT_L0("P2P Request: do_broadcast: prepare peerlist");

      std::string blob;
      epee::serialization::store_t_to_binary(p2p_req, blob);
      std::set<peerid_type> announced_peers;
      // send to peers
      m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& context) {
          MINFO("sending COMMAND_BROADCAST to " << context.peer_id);
          if (invoke_notify_to_peer(COMMAND_BROADCAST::ID, blob, context)) {
              announced_peers.insert(context.peer_id);
              return true;
          }
          return false;
      });

      std::list<peerlist_entry> peerlist_white, peerlist_gray;
      m_peerlist.get_peerlist_full(peerlist_white,peerlist_gray);
      std::vector<peerlist_entry> peers_to_send;
      for (auto pe :peerlist_white) {
          if (announced_peers.find(pe.id) != announced_peers.end())
              continue;
          peers_to_send.push_back(pe);
      }

      LOG_PRINT_L0("P2P Request: do_broadcast: notify_peer_list");
      notify_peer_list(COMMAND_BROADCAST::ID, blob, peers_to_send);
      LOG_PRINT_L0("P2P Request: do_broadcast: End");
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::do_multicast(const cryptonote::COMMAND_RPC_MULTICAST::request &req)
  {
      LOG_PRINT_L0("Incoming multicast request");

      LOG_PRINT_L0("P2P Request: do_multicast: Start");

      std::string data_blob;
      epee::serialization::store_t_to_binary(req, data_blob);
      std::vector<uint8_t> data_vec(data_blob.begin(), data_blob.end());
      crypto::hash message_hash;
      if (!tools::sha256sum(data_vec.data(), data_vec.size(), message_hash))
      {
          LOG_ERROR("RTA Multicast: wrong data format for hashing!");
          return;
      }

      LOG_PRINT_L0("P2P Request: do_multicast: multicast to me");
      {
          LOG_PRINT_L0("P2P Request: do_multicast: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
          LOG_PRINT_L0("P2P Request: do_multicast: unlock");
          std::list<std::string> addresses = req.receiver_addresses;
          auto it = std::find(addresses.begin(), addresses.end(), m_supernode_str);
          if (m_have_supernode && it != addresses.end())
          {
              post_request_to_supernode<cryptonote::COMMAND_RPC_MULTICAST>("multicast", req, req.callback_uri);
          }
      }

#ifdef LOCK_RTA_SENDING
    return;
#endif

      COMMAND_MULTICAST::request p2p_req = AUTO_VAL_INIT(p2p_req);
      p2p_req.receiver_addresses = req.receiver_addresses;
      p2p_req.sender_address = req.sender_address;
      p2p_req.callback_uri = req.callback_uri;
      p2p_req.data = req.data;
      p2p_req.wait_answer = req.wait_answer;
      p2p_req.hop = HOP_RETRIES_MULTIPLIER * get_max_hop(p2p_req.receiver_addresses);
      p2p_req.message_id = epee::string_tools::pod_to_hex(message_hash);

      LOG_PRINT_L0("P2P Request: do_multicast: multicast send");
      std::string blob;
      epee::serialization::store_t_to_binary(p2p_req, blob);
      multicast_send(COMMAND_MULTICAST::ID, blob, p2p_req.receiver_addresses);
      LOG_PRINT_L0("P2P Request: do_multicast: End");
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::do_unicast(const cryptonote::COMMAND_RPC_UNICAST::request &req)
  {
      LOG_PRINT_L0("Incoming unicast request");
#ifdef LOCK_RTA_SENDING
    return;
#endif

      LOG_PRINT_L0("P2P Request: do_unicast: Start sending to: " << req.receiver_address);

      std::list<std::string> addresses;
      addresses.push_back(req.receiver_address);

      std::string data_blob;
      epee::serialization::store_t_to_binary(req, data_blob);
      std::vector<uint8_t> data_vec(data_blob.begin(), data_blob.end());
      crypto::hash message_hash;
      if (!tools::sha256sum(data_vec.data(), data_vec.size(), message_hash))
      {
          LOG_ERROR("RTA Unicast: wrong data format for hashing!");
          return;
      }

      COMMAND_UNICAST::request p2p_req = AUTO_VAL_INIT(p2p_req);
      p2p_req.receiver_address = req.receiver_address;
      p2p_req.sender_address = req.sender_address;
      p2p_req.callback_uri = req.callback_uri;
      p2p_req.data = req.data;
      p2p_req.wait_answer = req.wait_answer;
      p2p_req.hop = HOP_RETRIES_MULTIPLIER * get_max_hop(addresses);
      p2p_req.message_id = epee::string_tools::pod_to_hex(message_hash);

      LOG_PRINT_L0("P2P Request: do_unicast: unicast send");
      std::string blob;
      epee::serialization::store_t_to_binary(p2p_req, blob);
      multicast_send(COMMAND_UNICAST::ID, blob, addresses);
      LOG_PRINT_L0("P2P Request: do_unicast: End");
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  std::vector<cryptonote::route_data> node_server<t_payload_net_handler>::get_tunnels() const
  {
      std::vector<cryptonote::route_data> tunnels;
      for (auto it = m_supernode_routes.begin(); it != m_supernode_routes.end(); ++it)
      {
          cryptonote::route_data route;
          route.address = it->first;
          route.last_announce_time = it->second.last_announce_time;
          route.max_hop = it->second.max_hop;
          std::vector<cryptonote::peer_data> peers;
          for (auto pit = it->second.peers.begin(); pit != it->second.peers.end(); ++pit)
          {
              cryptonote::peer_data peer;
              peer.host = pit->second.adr.host_str();
              peer.port = pit->second.adr.template as<epee::net_utils::ipv4_network_address>().port();
              peer.id = pit->second.id;
              peer.last_seen = pit->second.last_seen;
              peers.push_back(peer);
          }
          route.peers = peers;
          tunnels.push_back(route);
      }
      return tunnels;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  std::string node_server<t_payload_net_handler>::print_connections_container()
  {

    std::stringstream ss;
    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      ss << cntxt.m_remote_address.str()
        << " \t\tpeer_id " << cntxt.peer_id
        << " \t\tconn_id " << epee::string_tools::get_str_from_guid_a(cntxt.m_connection_id) << (cntxt.m_is_income ? " INC":" OUT")
        << std::endl;
      return true;
    });
    std::string s = ss.str();
    return s;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::on_connection_new(p2p_connection_context& context)
  {
    MINFO("["<< epee::net_utils::print_connection_context(context) << "] NEW CONNECTION");
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::on_connection_close(p2p_connection_context& context)
  {
    if (!m_net_server.is_stop_signal_sent() && !context.m_is_income) {
      epee::net_utils::network_address na = AUTO_VAL_INIT(na);
      na = context.m_remote_address;

      m_peerlist.remove_from_peer_anchor(na);
    }

    m_payload_handler.on_connection_close(context);

    MINFO("["<< epee::net_utils::print_connection_context(context) << "] CLOSE CONNECTION");
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_priority_node(const epee::net_utils::network_address& na)
  {
    return (std::find(m_priority_peers.begin(), m_priority_peers.end(), na) != m_priority_peers.end()) || (std::find(m_exclusive_peers.begin(), m_exclusive_peers.end(), na) != m_exclusive_peers.end());
  }

  template<class t_payload_net_handler> template <class Container>
  bool node_server<t_payload_net_handler>::connect_to_peerlist(const Container& peers)
  {
    for(const epee::net_utils::network_address& na: peers)
    {
      if(m_net_server.is_stop_signal_sent())
        return false;

      if(is_addr_connected(na))
        continue;

      try_to_connect_and_handshake_with_new_peer(na);
    }

    return true;
  }

  template<class t_payload_net_handler> template <class Container>
  bool node_server<t_payload_net_handler>::parse_peers_and_add_to_container(const boost::program_options::variables_map& vm, const command_line::arg_descriptor<std::vector<std::string> > & arg, Container& container)
  {
    std::vector<std::string> perrs = command_line::get_arg(vm, arg);

    for(const std::string& pr_str: perrs)
    {
      epee::net_utils::network_address na = AUTO_VAL_INIT(na);
      const uint16_t default_port = m_testnet ? ::config::testnet::P2P_DEFAULT_PORT : ::config::P2P_DEFAULT_PORT;
      bool r = parse_peer_from_string(na, pr_str, default_port);
      CHECK_AND_ASSERT_MES(r, false, "Failed to parse address from string: " << pr_str);
      container.push_back(na);
    }

    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_max_out_peers(const boost::program_options::variables_map& vm, int64_t max)
  {
    if(max == -1) {
      m_config.m_net_config.connections_count = P2P_DEFAULT_CONNECTIONS_COUNT;
      return true;
    }
    m_config.m_net_config.connections_count = max;
    return true;
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::delete_connections(size_t count)
  {
    m_net_server.get_config_object().del_out_connections(count);
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_tos_flag(const boost::program_options::variables_map& vm, int flag)
  {
    if(flag==-1){
      return true;
    }
    epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_tos_flag(flag);
    _dbg1("Set ToS flag  " << flag);
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_rate_up_limit(const boost::program_options::variables_map& vm, int64_t limit)
  {
    this->islimitup=true;

    if (limit==-1) {
      limit=default_limit_up;
      this->islimitup=false;
    }

    limit *= 1024;
    epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_rate_up_limit( limit );
    MINFO("Set limit-up to " << limit/1024 << " kB/s");
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_rate_down_limit(const boost::program_options::variables_map& vm, int64_t limit)
  {
    this->islimitdown=true;
    if(limit==-1) {
      limit=default_limit_down;
      this->islimitdown=false;
    }
    limit *= 1024;
    epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_rate_down_limit( limit );
    MINFO("Set limit-down to " << limit/1024 << " kB/s");
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_rate_limit(const boost::program_options::variables_map& vm, int64_t limit)
  {
    int64_t limit_up = 0;
    int64_t limit_down = 0;

    if(limit == -1)
    {
      limit_up = default_limit_up * 1024;
      limit_down = default_limit_down * 1024;
    }
    else
    {
      limit_up = limit * 1024;
      limit_down = limit * 1024;
    }
    if(!this->islimitup) {
      epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_rate_up_limit(limit_up);
      MINFO("Set limit-up to " << limit_up/1024 << " kB/s");
    }
    if(!this->islimitdown) {
      epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_rate_down_limit(limit_down);
      MINFO("Set limit-down to " << limit_down/1024 << " kB/s");
    }

    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::has_too_many_connections(const epee::net_utils::network_address &address)
  {
    const uint8_t max_connections = 1;
    uint8_t count = 0;

    m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if (cntxt.m_is_income && cntxt.m_remote_address.is_same_host(address)) {
        count++;

        if (count > max_connections) {
          return false;
        }
      }

      return true;
    });

    return count > max_connections;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::gray_peerlist_housekeeping()
  {
    peerlist_entry pe = AUTO_VAL_INIT(pe);

    if (!m_peerlist.get_random_gray_peer(pe)) {
        return false;
    }

    bool success = check_connection_and_handshake_with_peer(pe.adr, pe.last_seen);

    if (!success) {
      m_peerlist.remove_from_peer_gray(pe);

      LOG_PRINT_L2("PEER EVICTED FROM GRAY PEER LIST IP address: " << pe.adr.host_str() << " Peer ID: " << peerid_type(pe.id));

      return true;
    }

    m_peerlist.set_peer_just_seen(pe.id, pe.adr);

    LOG_PRINT_L2("PEER PROMOTED TO WHITE PEER LIST IP address: " << pe.adr.host_str() << " Peer ID: " << peerid_type(pe.id));

    return true;
  }
}
