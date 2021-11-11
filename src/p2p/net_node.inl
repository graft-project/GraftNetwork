// Copyright (c) 2018, The Graft Project
// Copyright (c) 2014-2019, The Monero Project
// Copyright (c) 2018-2019, The Loki Project
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

#include <algorithm>
#include <atomic>
#include <chrono>

#include <boost/bind/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/optional/optional.hpp>
#include <boost/thread/thread.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <atomic>
#include <random>
#include <boost/algorithm/string/join.hpp> // for logging
#include <functional>
#include <limits>
#include <memory>
#include <tuple>
#include <vector>

#include "version.h"
#include "string_tools.h"
#include "common/util.h"
#include "common/dns_utils.h"
#include "common/pruning.h"
#include "net/error.h"
#include "net/net_helper.h"
#include "math_helper.h"
#include "misc_log_ex.h"
#include "p2p_protocol_defs.h"
#include "net/local_ip.h"
#include "crypto/crypto.h"
#include "storages/levin_abstract_invoke2.h"
#include "cryptonote_core/cryptonote_core.h"
#include "net/parse.h"

#include <miniupnp/miniupnpc/miniupnpc.h>
#include <miniupnp/miniupnpc/upnpcommands.h>
#include <miniupnp/miniupnpc/upnperrors.h>

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "net.p2p"

#define NET_MAKE_IP(b1,b2,b3,b4)  ((LPARAM)(((DWORD)(b1)<<24)+((DWORD)(b2)<<16)+((DWORD)(b3)<<8)+((DWORD)(b4))))

#define MIN_WANTED_SEED_NODES 12

#define MAX_TUNNEL_PEERS (3u)
#define REQUEST_CACHE_TIME 2 * 60 * 1000
#define HOP_RETRIES_MULTIPLIER 2

namespace nodetool
{
  template<class t_payload_net_handler>
  node_server<t_payload_net_handler>::~node_server()
  {
    // tcp server uses io_service in destructor, and every zone uses
    // io_service from public zone.
    for (auto current = m_network_zones.begin(); current != m_network_zones.end(); /* below */)
    {
      if (current->first != epee::net_utils::zone::public_)
        current = m_network_zones.erase(current);
      else
        ++current;
    }
  }
  //-----------------------------------------------------------------------------------
  inline bool append_net_address(std::vector<epee::net_utils::network_address> & seed_nodes, std::string const & addr, uint16_t default_port);

  namespace
  {


    // helper struct used to notify peers by uuid
    struct connection_info
    {
        boost::uuids::uuid id;
        peerid_type peer_id;
        std::string info;
    };

    /*!
     * helper to return non-empty subset of 'in' container, each element included with probability 'p'
     */
    template <typename T>
    void select_subset_with_probability(double p, const T &in, T &out)
    {
        if (in.empty())
            return;
        static auto gen = std::mt19937{std::random_device{}()};
        do {
            for (const auto & item : in) {
                std::uniform_real_distribution<> urd(0, 1);
                auto rand = urd(gen);
                if (rand < p) {
                    out.push_back(std::move(item));
                }
            }
        } while (out.empty());
    }
    /*!
     * helper to calculate p2p command size in bytes
     */
    template <typename T>
    size_t get_command_size(const T &arg)
    {
        std::string buff;
        epee::serialization::store_t_to_binary(arg, buff);
        return buff.size();
    }

  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_p2p_bind_ip);
    command_line::add_arg(desc, arg_p2p_bind_ipv6_address);
    command_line::add_arg(desc, arg_p2p_bind_port, false);
    command_line::add_arg(desc, arg_p2p_bind_port_ipv6, false);
    command_line::add_arg(desc, arg_p2p_use_ipv6);
    command_line::add_arg(desc, arg_p2p_require_ipv4);
    command_line::add_arg(desc, arg_p2p_external_port);
    command_line::add_arg(desc, arg_p2p_allow_local_ip);
    command_line::add_arg(desc, arg_p2p_add_peer);
    command_line::add_arg(desc, arg_p2p_add_priority_node);
    command_line::add_arg(desc, arg_p2p_add_exclusive_node);
    command_line::add_arg(desc, arg_p2p_seed_node);
    command_line::add_arg(desc, arg_proxy);
    command_line::add_arg(desc, arg_anonymous_inbound);
    command_line::add_arg(desc, arg_p2p_hide_my_port);
    command_line::add_arg(desc, arg_no_sync);
    command_line::add_arg(desc, arg_no_igd);
    command_line::add_arg(desc, arg_igd);
    command_line::add_arg(desc, arg_out_peers);
    command_line::add_arg(desc, arg_in_peers);
    command_line::add_arg(desc, arg_tos_flag);
    command_line::add_arg(desc, arg_limit_rate_up);
    command_line::add_arg(desc, arg_limit_rate_down);
    command_line::add_arg(desc, arg_limit_rate);
    // TODO: Graft: remove two args below?
    command_line::add_arg(desc, arg_save_graph);
    command_line::add_arg(desc, arg_p2p_net_id);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::init_config()
  {
    TRY_ENTRY();
    auto storage = peerlist_storage::open(m_config_folder + "/" + P2P_NET_DATA_FILENAME);
    if (storage)
      m_peerlist_storage = std::move(*storage);

    m_network_zones[epee::net_utils::zone::public_].m_config.m_support_flags = P2P_SUPPORT_FLAGS;
    m_first_connection_maker_call = true;

    CATCH_ENTRY_L0("node_server::init_config", false);
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::for_each_connection(std::function<bool(typename t_payload_net_handler::connection_context&, peerid_type, uint32_t)> f)
  {
    for(auto& zone : m_network_zones)
    {
      zone.second.m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& cntx){
        return f(cntx, cntx.peer_id, cntx.support_flags);
      });
    }
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::for_connection(const boost::uuids::uuid &connection_id, std::function<bool(typename t_payload_net_handler::connection_context&, peerid_type, uint32_t)> f)
  {
    for(auto& zone : m_network_zones)
    {
      const bool result = zone.second.m_net_server.get_config_object().for_connection(connection_id, [&](p2p_connection_context& cntx){
        return f(cntx, cntx.peer_id, cntx.support_flags);
      });
      if (result)
        return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_remote_host_allowed(const epee::net_utils::network_address &address, time_t *t)
  {
    CRITICAL_REGION_LOCAL(m_blocked_hosts_lock);

    const time_t now = time(nullptr);

    // look in the hosts list
    auto it = m_blocked_hosts.find(address.host_str());
    if (it != m_blocked_hosts.end())
    {
      if (now >= it->second)
      {
        m_blocked_hosts.erase(it);
        MCLOG_CYAN(el::Level::Info, "global", "Host " << address.host_str() << " unblocked.");
        it = m_blocked_hosts.end();
      }
      else
      {
        if (t)
          *t = it->second - now;
        return false;
      }
    }

    // manually loop in subnets
    if (address.get_type_id() == epee::net_utils::address_type::ipv4)
    {
      auto ipv4_address = address.template as<epee::net_utils::ipv4_network_address>();
      std::map<epee::net_utils::ipv4_network_subnet, time_t>::iterator it;
      for (it = m_blocked_subnets.begin(); it != m_blocked_subnets.end(); )
      {
        if (now >= it->second)
        {
          it = m_blocked_subnets.erase(it);
          MCLOG_CYAN(el::Level::Info, "global", "Subnet " << it->first.host_str() << " unblocked.");
          continue;
        }
        if (it->first.matches(ipv4_address))
        {
          if (t)
            *t = it->second - now;
          return false;
        }
        ++it;
      }
    }

    // not found in hosts or subnets, allowed
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::block_host(const epee::net_utils::network_address &addr, time_t seconds)
  {
    if(!addr.is_blockable())
      return false;

    const time_t now = time(nullptr);

    CRITICAL_REGION_LOCAL(m_blocked_hosts_lock);
    time_t limit;
    if (now > std::numeric_limits<time_t>::max() - seconds)
      limit = std::numeric_limits<time_t>::max();
    else
      limit = now + seconds;
    m_blocked_hosts[addr.host_str()] = limit;

    // drop any connection to that address. This should only have to look into
    // the zone related to the connection, but really make sure everything is
    // swept ...
    std::vector<boost::uuids::uuid> conns;
    for(auto& zone : m_network_zones)
    {
      zone.second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
      {
        if (cntxt.m_remote_address.is_same_host(addr))
        {
          conns.push_back(cntxt.m_connection_id);
        }
        return true;
      });
      for (const auto &c: conns)
        zone.second.m_net_server.get_config_object().close(c);

      conns.clear();
    }

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
  bool node_server<t_payload_net_handler>::block_subnet(const epee::net_utils::ipv4_network_subnet &subnet, time_t seconds)
  {
    const time_t now = time(nullptr);

    CRITICAL_REGION_LOCAL(m_blocked_hosts_lock);
    time_t limit;
    if (now > std::numeric_limits<time_t>::max() - seconds)
      limit = std::numeric_limits<time_t>::max();
    else
      limit = now + seconds;
    m_blocked_subnets[subnet] = limit;

    // drop any connection to that subnet. This should only have to look into
    // the zone related to the connection, but really make sure everything is
    // swept ...
    std::vector<boost::uuids::uuid> conns;
    for(auto& zone : m_network_zones)
    {
      zone.second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
      {
        if (cntxt.m_remote_address.get_type_id() != epee::net_utils::ipv4_network_address::get_type_id())
          return true;
        auto ipv4_address = cntxt.m_remote_address.template as<epee::net_utils::ipv4_network_address>();
        if (subnet.matches(ipv4_address))
        {
          conns.push_back(cntxt.m_connection_id);
        }
        return true;
      });
      for (const auto &c: conns)
        zone.second.m_net_server.get_config_object().close(c);

      conns.clear();
    }

    MCLOG_CYAN(el::Level::Info, "global", "Subnet " << subnet.host_str() << " blocked.");
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::unblock_subnet(const epee::net_utils::ipv4_network_subnet &subnet)
  {
    CRITICAL_REGION_LOCAL(m_blocked_hosts_lock);
    auto i = m_blocked_subnets.find(subnet);
    if (i == m_blocked_subnets.end())
      return false;
    m_blocked_subnets.erase(i);
    MCLOG_CYAN(el::Level::Info, "global", "Subnet " << subnet.host_str() << " unblocked.");
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::add_host_fail(const epee::net_utils::network_address &address)
  {
    if(!address.is_blockable())
      return false;

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
  bool node_server<t_payload_net_handler>::handle_command_line(
      const boost::program_options::variables_map& vm
    )
  {
    bool testnet = command_line::get_arg(vm, cryptonote::arg_testnet_on);
    bool stagenet = command_line::get_arg(vm, cryptonote::arg_stagenet_on);
    bool fakenet = command_line::get_arg(vm, cryptonote::arg_regtest_on);
    m_nettype =
        testnet  ? cryptonote::TESTNET :
        stagenet ? cryptonote::STAGENET :
        fakenet  ? cryptonote::FAKECHAIN :
        cryptonote::MAINNET;

    network_zone& public_zone = m_network_zones[epee::net_utils::zone::public_];
    public_zone.m_connect = &public_connect;
    public_zone.m_bind_ip = command_line::get_arg(vm, arg_p2p_bind_ip);
    public_zone.m_bind_ipv6_address = command_line::get_arg(vm, arg_p2p_bind_ipv6_address);
    public_zone.m_port = command_line::get_arg(vm, arg_p2p_bind_port);
    public_zone.m_port_ipv6 = command_line::get_arg(vm, arg_p2p_bind_port_ipv6);
    public_zone.m_can_pingback = true;
    m_external_port = command_line::get_arg(vm, arg_p2p_external_port);
    m_allow_local_ip = command_line::get_arg(vm, arg_p2p_allow_local_ip);
    const bool has_no_igd = command_line::get_arg(vm, arg_no_igd);
    const std::string sigd = command_line::get_arg(vm, arg_igd);
    if (sigd == "enabled")
    {
      if (has_no_igd)
      {
        MFATAL("Cannot have both --" << arg_no_igd.name << " and --" << arg_igd.name << " enabled");
        return false;
      }
      m_igd = igd;
    }
    else if (sigd == "disabled")
    {
      m_igd =  no_igd;
    }
    else if (sigd == "delayed")
    {
      if (has_no_igd && !command_line::is_arg_defaulted(vm, arg_igd))
      {
        MFATAL("Cannot have both --" << arg_no_igd.name << " and --" << arg_igd.name << " delayed");
        return false;
      }
      m_igd = has_no_igd ? no_igd : delayed_igd;
    }
    else
    {
      MFATAL("Invalid value for --" << arg_igd.name << ", expected enabled, disabled or delayed");
      return false;
    }
    m_offline = command_line::get_arg(vm, cryptonote::arg_offline);
    m_use_ipv6 = command_line::get_arg(vm, arg_p2p_use_ipv6);
    m_require_ipv4 = command_line::get_arg(vm, arg_p2p_require_ipv4);

    if (command_line::has_arg(vm, arg_p2p_add_peer))
    {
      std::vector<std::string> perrs = command_line::get_arg(vm, arg_p2p_add_peer);
      for(const std::string& pr_str: perrs)
      {
        nodetool::peerlist_entry pe{};
        pe.id = crypto::rand<uint64_t>();
        const uint16_t default_port = cryptonote::get_config(m_nettype).P2P_DEFAULT_PORT;
        expect<epee::net_utils::network_address> adr = net::get_network_address(pr_str, default_port);
        if (adr)
        {
          add_zone(adr->get_zone());
          pe.adr = std::move(*adr);
          m_command_line_peers.push_back(std::move(pe));
          continue;
        }
        CHECK_AND_ASSERT_MES(
          adr == net::error::unsupported_address, false, "Bad address (\"" << pr_str << "\"): " << adr.error().message()
        );

        std::vector<epee::net_utils::network_address> resolved_addrs;
        bool r = append_net_address(resolved_addrs, pr_str, default_port);
        CHECK_AND_ASSERT_MES(r, false, "Failed to parse or resolve address from string: " << pr_str);
        for (const epee::net_utils::network_address& addr : resolved_addrs)
        {
          pe.id = crypto::rand<uint64_t>();
          pe.adr = addr;
          m_command_line_peers.push_back(pe);
        }
      }
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

    if (command_line::has_arg(vm, arg_p2p_seed_node))
    {
      m_custom_seed_nodes.clear();
      if (!parse_peers_and_add_to_container(vm, arg_p2p_seed_node, m_custom_seed_nodes))
        return false;
    }

    if(command_line::has_arg(vm, arg_p2p_hide_my_port))
      m_hide_my_port = true;

    if (command_line::has_arg(vm, arg_no_sync))
      m_payload_handler.set_no_sync(true);

    if ( !set_max_out_peers(public_zone, command_line::get_arg(vm, arg_out_peers) ) )
      return false;
    else
      m_payload_handler.set_max_out_peers(public_zone.m_config.m_net_config.max_out_connection_count);


    if ( !set_max_in_peers(public_zone, command_line::get_arg(vm, arg_in_peers) ) )
      return false;

    if ( !set_tos_flag(vm, command_line::get_arg(vm, arg_tos_flag) ) )
      return false;

    if ( !set_rate_up_limit(vm, command_line::get_arg(vm, arg_limit_rate_up) ) )
      return false;

    if ( !set_rate_down_limit(vm, command_line::get_arg(vm, arg_limit_rate_down) ) )
      return false;

    if ( !set_rate_limit(vm, command_line::get_arg(vm, arg_limit_rate) ) )
      return false;


    auto proxies = get_proxies(vm);
    if (!proxies)
      return false;

    for (auto& proxy : *proxies)
    {
      network_zone& zone = add_zone(proxy.zone);
      if (zone.m_connect != nullptr)
      {
        MERROR("Listed --" << arg_proxy.name << " twice with " << epee::net_utils::zone_to_string(proxy.zone));
        return false;
      }
      zone.m_connect = &socks_connect;
      zone.m_proxy_address = std::move(proxy.address);

      if (!set_max_out_peers(zone, proxy.max_connections))
        return false;
    }

    for (const auto& zone : m_network_zones)
    {
      if (zone.second.m_connect == nullptr)
      {
        MERROR("Set outgoing peer for " << epee::net_utils::zone_to_string(zone.first) << " but did not set --" << arg_proxy.name);
        return false;
      }
    }

    auto inbounds = get_anonymous_inbounds(vm);
    if (!inbounds)
      return false;

    for (auto& inbound : *inbounds)
    {
      network_zone& zone = add_zone(inbound.our_address.get_zone());

      if (!zone.m_bind_ip.empty())
      {
        MERROR("Listed --" << arg_anonymous_inbound.name << " twice with " << epee::net_utils::zone_to_string(inbound.our_address.get_zone()) << " network");
        return false;
      }

      zone.m_bind_ip = std::move(inbound.local_ip);
      zone.m_port = std::move(inbound.local_port);
      zone.m_net_server.set_default_remote(std::move(inbound.default_remote));
      zone.m_our_address = std::move(inbound.our_address);

      if (!set_max_in_peers(zone, inbound.max_connections))
        return false;
    }

    return true;
  }
  //-----------------------------------------------------------------------------------
  inline bool append_net_address(
      std::vector<epee::net_utils::network_address> & seed_nodes
    , std::string const & addr
    , uint16_t default_port
    )
  {
    using namespace boost::asio;

    std::string host = addr;
    std::string port = std::to_string(default_port);
    size_t colon_pos = addr.find_last_of(':');
    size_t dot_pos = addr.find_last_of('.');
    size_t square_brace_pos = addr.find('[');

    // IPv6 will have colons regardless.  IPv6 and IPv4 address:port will have a colon but also either a . or a [
    // as IPv6 addresses specified as address:port are to be specified as "[addr:addr:...:addr]:port"
    // One may also specify an IPv6 address as simply "[addr:addr:...:addr]" without the port; in that case
    // the square braces will be stripped here.
    if ((std::string::npos != colon_pos && std::string::npos != dot_pos) || std::string::npos != square_brace_pos)
    {
      net::get_network_address_host_and_port(addr, host, port);
    }
    MINFO("Resolving node address: host=" << host << ", port=" << port);

    io_service io_srv;
    ip::tcp::resolver resolver(io_srv);
    ip::tcp::resolver::query query(host, port, boost::asio::ip::tcp::resolver::query::canonical_name);
    boost::system::error_code ec;
    ip::tcp::resolver::iterator i = resolver.resolve(query, ec);
    CHECK_AND_ASSERT_MES(!ec, false, "Failed to resolve host name '" << host << "': " << ec.message() << ':' << ec.value());

    ip::tcp::resolver::iterator iend;
    for (; i != iend; ++i)
    {
      ip::tcp::endpoint endpoint = *i;
      if (endpoint.address().is_v4())
      {
        epee::net_utils::network_address na{epee::net_utils::ipv4_network_address{boost::asio::detail::socket_ops::host_to_network_long(endpoint.address().to_v4().to_ulong()), endpoint.port()}};
        seed_nodes.push_back(na);
        MINFO("Added node: " << na.str());
      }
      else
      {
        epee::net_utils::network_address na{epee::net_utils::ipv6_network_address{endpoint.address().to_v6(), endpoint.port()}};
        seed_nodes.push_back(na);
        MINFO("Added node: " << na.str());
      }
    }
    return true;
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  std::set<std::string> node_server<t_payload_net_handler>::get_seed_nodes(cryptonote::network_type nettype) const
  {
    std::set<std::string> full_addrs;
    
    if (nettype == cryptonote::TESTNET)
    {
      if (m_custom_seed_nodes.size() > 0)
      {
          for (const auto& na : m_custom_seed_nodes)
          {
              const auto& ipv4 = na.template as<const epee::net_utils::ipv4_network_address>();
              full_addrs.insert(epee::string_tools::get_ip_string_from_int32(ipv4.ip()) + ":"
                                + epee::string_tools::num_to_string_fast(ipv4.port()) );
          }
      } else {
        full_addrs.insert("212.71.237.82:28880");
        full_addrs.insert("45.79.47.118:28880");
        full_addrs.insert("139.162.61.111:28880");
      }
    }
    else if (nettype == cryptonote::STAGENET)
    {
    }
    else if (nettype == cryptonote::FAKECHAIN)
    {
    }
    else
    {
      full_addrs.insert("109.74.204.179:18980");
      full_addrs.insert("45.79.42.116:18980");
      full_addrs.insert("207.148.153.14:18980");
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
  typename node_server<t_payload_net_handler>::network_zone& node_server<t_payload_net_handler>::add_zone(const epee::net_utils::zone zone)
  {
    const auto zone_ = m_network_zones.lower_bound(zone);
    if (zone_ != m_network_zones.end() && zone_->first == zone)
      return zone_->second;

    network_zone& public_zone = m_network_zones[epee::net_utils::zone::public_];
    return m_network_zones.emplace_hint(zone_, std::piecewise_construct, std::make_tuple(zone), std::tie(public_zone.m_net_server.get_io_service()))->second;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::init(const boost::program_options::variables_map& vm)
  {
#if 0  // TODO: Graft: del
    m_payload_handler.get_core().set_update_stakes_handler(
      [&](uint64_t block_height, const cryptonote::StakeTransactionProcessor::supernode_stake_array& stakes) { handle_stakes_update(block_height, stakes); }
    );

    m_payload_handler.get_core().set_update_blockchain_based_list_handler(
      [&](uint64_t block_height, const cryptonote::StakeTransactionProcessor::supernode_tier_array& tiers) { handle_blockchain_based_list_update(block_height, tiers); }
    );
#endif
    

    std::set<std::string> full_addrs;

    bool res = handle_command_line(vm);
    CHECK_AND_ASSERT_MES(res, false, "Failed to handle command line");

    m_fallback_seed_nodes_added = false;
    if (m_nettype == cryptonote::TESTNET)
    {
      m_custom_seed_nodes.clear();
      parse_peers_and_add_to_container(vm, arg_p2p_seed_node, m_custom_seed_nodes);
      assign_network_id(vm, true, m_network_id);
      full_addrs = get_seed_nodes(cryptonote::TESTNET);  
    }
    else if (m_nettype == cryptonote::STAGENET)
    {
      memcpy(&m_network_id, &::config::stagenet::NETWORK_ID, 16);
      full_addrs = get_seed_nodes(cryptonote::STAGENET);
    }
    else
    {
      assign_network_id(vm, false, m_network_id);
      if (m_exclusive_peers.empty() && !m_offline)
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
            full_addrs.insert(addr_string + ":" + std::to_string(cryptonote::get_config(m_nettype).P2P_DEFAULT_PORT));
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

        for (const auto &peer: get_seed_nodes(cryptonote::MAINNET))
          full_addrs.insert(peer);
        m_fallback_seed_nodes_added = true;
      }
    }
    }

    for (const auto& full_addr : full_addrs)
    {
      MDEBUG("Seed node: " << full_addr);
      append_net_address(m_seed_nodes, full_addr, cryptonote::get_config(m_nettype).P2P_DEFAULT_PORT);
    }
    MDEBUG("Number of seed nodes: " << m_seed_nodes.size());

    m_config_folder = command_line::get_arg(vm, cryptonote::arg_data_dir);
    network_zone& public_zone = m_network_zones.at(epee::net_utils::zone::public_);

    if ((m_nettype == cryptonote::MAINNET && public_zone.m_port != std::to_string(::config::P2P_DEFAULT_PORT))
        || (m_nettype == cryptonote::TESTNET && public_zone.m_port != std::to_string(::config::testnet::P2P_DEFAULT_PORT))
        || (m_nettype == cryptonote::STAGENET && public_zone.m_port != std::to_string(::config::stagenet::P2P_DEFAULT_PORT))) {
      m_config_folder = m_config_folder + "/" + public_zone.m_port;
    }

    res = init_config();
    CHECK_AND_ASSERT_MES(res, false, "Failed to init config.");

    for (auto& zone : m_network_zones)
    {
      res = zone.second.m_peerlist.init(m_peerlist_storage.take_zone(zone.first), m_allow_local_ip);
      CHECK_AND_ASSERT_MES(res, false, "Failed to init peerlist.");
    }

    for(const auto& p: m_command_line_peers)
      m_network_zones.at(p.adr.get_zone()).m_peerlist.append_with_peer_white(p);

// all peers are now setup
#ifdef CRYPTONOTE_PRUNING_DEBUG_SPOOF_SEED
    for (auto& zone : m_network_zones)
    {
      std::list<peerlist_entry> plw;
      while (zone.second.m_peerlist.get_white_peers_count())
      {
        plw.push_back(peerlist_entry());
        zone.second.m_peerlist.get_white_peer_by_index(plw.back(), 0);
        zone.second.m_peerlist.remove_from_peer_white(plw.back());
      }
      for (auto &e:plw)
        zone.second.m_peerlist.append_with_peer_white(e);

      std::list<peerlist_entry> plg;
      while (zone.second.m_peerlist.get_gray_peers_count())
      {
        plg.push_back(peerlist_entry());
        zone.second.m_peerlist.get_gray_peer_by_index(plg.back(), 0);
        zone.second.m_peerlist.remove_from_peer_gray(plg.back());
      }
      for (auto &e:plg)
        zone.second.m_peerlist.append_with_peer_gray(e);
    }
#endif

    //only in case if we really sure that we have external visible ip
    m_have_address = true;
    m_last_stat_request_time = 0;

    //configure self

    public_zone.m_net_server.set_threads_prefix("P2P"); // all zones use these threads/asio::io_service

    // from here onwards, it's online stuff
    if (m_offline)
      return res;

    //try to bind
    m_ssl_support = epee::net_utils::ssl_support_t::e_ssl_support_disabled;
    for (auto& zone : m_network_zones)
    {
      zone.second.m_net_server.get_config_object().set_handler(this);
      zone.second.m_net_server.get_config_object().m_invoke_timeout = P2P_DEFAULT_INVOKE_TIMEOUT;

      if (!zone.second.m_bind_ip.empty())
      {
        std::string ipv6_addr = "";
        std::string ipv6_port = "";
        zone.second.m_net_server.set_connection_filter(this);
        MINFO("Binding (IPv4) on " << zone.second.m_bind_ip << ":" << zone.second.m_port);
        if (!zone.second.m_bind_ipv6_address.empty() && m_use_ipv6)
        {
          ipv6_addr = zone.second.m_bind_ipv6_address;
          ipv6_port = zone.second.m_port_ipv6;
          MINFO("Binding (IPv6) on " << zone.second.m_bind_ipv6_address << ":" << zone.second.m_port_ipv6);
        }
        res = zone.second.m_net_server.init_server(zone.second.m_port, zone.second.m_bind_ip, ipv6_port, ipv6_addr, m_use_ipv6, m_require_ipv4, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
        CHECK_AND_ASSERT_MES(res, false, "Failed to bind server");
      }
    }

    m_listening_port = public_zone.m_net_server.get_binded_port();
    MLOG_GREEN(el::Level::Info, "Net service bound (IPv4) to " << public_zone.m_bind_ip << ":" << m_listening_port);
    if (m_use_ipv6)
    {
      m_listening_port_ipv6 = public_zone.m_net_server.get_binded_port_ipv6();
      MLOG_GREEN(el::Level::Info, "Net service bound (IPv6) to " << public_zone.m_bind_ipv6_address << ":" << m_listening_port_ipv6);
    }
    if(m_external_port)
      MDEBUG("External port defined as " << m_external_port);

    // add UPnP port mapping
    if(m_igd == igd)
    {
      add_upnp_port_mapping_v4(m_listening_port);
      if (m_use_ipv6)
      {
        add_upnp_port_mapping_v6(m_listening_port_ipv6);
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
      const network_zone& public_zone = m_network_zones.at(epee::net_utils::zone::public_);
      while (!is_closing && !public_zone.m_net_server.is_stop_signal_sent())
      { // main loop of thread
        //number_of_peers = m_net_server.get_config_object().get_connections_count();
        for (auto& zone : m_network_zones)
        {
          unsigned int number_of_in_peers = 0;
          unsigned int number_of_out_peers = 0;
          zone.second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
          {
            if (cntxt.m_is_income)
            {
              ++number_of_in_peers;
            }
            else
            {
              // If this is a new (<10s) connection and we're still in before handshake mode then
              // don't count it yet: it is probably a back ping connection that will be closed soon.
              if (!(cntxt.m_state == p2p_connection_context::state_before_handshake && std::time(NULL) < cntxt.m_started + 10))
                ++number_of_out_peers;
            }
            return true;
          }); // lambda
          zone.second.m_current_number_of_in_peers = number_of_in_peers;
          zone.second.m_current_number_of_out_peers = number_of_out_peers;
        }
        boost::this_thread::sleep_for(boost::chrono::seconds(1));
      } // main loop of thread
      _note("Thread monitor number of peers - done");
    })); // lambda

    network_zone& public_zone = m_network_zones.at(epee::net_utils::zone::public_);
    public_zone.m_net_server.add_idle_handler(boost::bind(&node_server<t_payload_net_handler>::idle_worker, this), 1000);
    public_zone.m_net_server.add_idle_handler(boost::bind(&t_payload_net_handler::on_idle, &m_payload_handler), 1000);

    //here you can set worker threads count
    int thrds_count = 10;
    boost::thread::attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);
    //go to loop
    MINFO("Run net_service loop( " << thrds_count << " threads)...");
    if(!public_zone.m_net_server.run_server(thrds_count, true, attrs))
    {
      LOG_ERROR("Failed to run net tcp server!");
    }

    MINFO("net_service loop stopped.");
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  uint64_t node_server<t_payload_net_handler>::get_public_connections_count()
  {
    auto public_zone = m_network_zones.find(epee::net_utils::zone::public_);
    if (public_zone == m_network_zones.end())
      return 0;
    return public_zone->second.m_net_server.get_config_object().get_connections_count();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  uint64_t node_server<t_payload_net_handler>::get_connections_count()
  {
    std::uint64_t count = 0;
    for (auto& zone : m_network_zones)
      count += zone.second.m_net_server.get_config_object().get_connections_count();
    return count;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::deinit()
  {
    kill();

    if (!m_offline)
    {
      for(auto& zone : m_network_zones)
        zone.second.m_net_server.deinit_server();
      // remove UPnP port mapping
      if(m_igd == igd)
        delete_upnp_port_mapping(m_listening_port);
    }
    return store_config();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::store_config()
  {
    TRY_ENTRY();

    if (!tools::create_directories_if_necessary(m_config_folder))
    {
      MWARNING("Failed to create data directory \"" << m_config_folder);
      return false;
    }

    peerlist_types active{};
    for (auto& zone : m_network_zones)
      zone.second.m_peerlist.get_peerlist(active);

    const std::string state_file_path = m_config_folder + "/" + P2P_NET_DATA_FILENAME;
    if (!m_peerlist_storage.store(state_file_path, active))
    {
      MWARNING("Failed to save config to file " << state_file_path);
      return false;
    }
    CATCH_ENTRY_L0("node_server::store", false);
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::send_stop_signal()
  {
    MDEBUG("[node] sending stop signal");
    for (auto& zone : m_network_zones)
        zone.second.m_net_server.send_stop_signal();
    MDEBUG("[node] Stop signal sent");

    for (auto& zone : m_network_zones)
    {
      std::list<boost::uuids::uuid> connection_ids;
      zone.second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt) {
        connection_ids.push_back(cntxt.m_connection_id);
        return true;
      });
      for (const auto &connection_id: connection_ids)
        zone.second.m_net_server.get_config_object().close(connection_id);
    }
    m_payload_handler.stop();
    return true;
  }
#if 0 // TODO: Graft: del
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::notify_peer_list(int command, const std::string& buf, const std::vector<peerlist_entry>& peers_to_send, bool try_connect)
  {
      MDEBUG("P2P Request: notify_peer_list: start notify, total peers: " << peers_to_send.size());
      for (unsigned i = 0; i < peers_to_send.size(); i++) {
          const peerlist_entry &pe = peers_to_send[i];
          boost::uuids::uuid conn_id;
          MDEBUG("P2P Request: notify_peer_list: start notify: looking for existing connection for peer: " << pe.id << ", [" << pe.adr.str() << "]");
          bool connection_exists = find_connection_id_by_peer(pe, conn_id);

          bool sent = false;
          MDEBUG("P2P Request: notify_peer_list: sending to peer: " << pe.adr.str()  << ", already connected: " << connection_exists
                       << ", try connect: " << try_connect);
          if (connection_exists) {
              MDEBUG("P2P Request: notify_peer_list: peer is connected, sending to : " << pe.adr.host_str());
              sent = relay_notify(command, buf, conn_id);
              if (!sent)
                MWARNING("P2P Request: notify_peer_list: peer is connected, sending to : " << pe.adr.host_str() << " FAILED");
          } else if (try_connect) {
              MDEBUG("P2P Request: notify_peer_list: connect to notify");
              const epee::net_utils::network_address& na = pe.adr;
              const epee::net_utils::ipv4_network_address &ipv4 = na.as<const epee::net_utils::ipv4_network_address>();
              typename net_server::t_connection_context con = AUTO_VAL_INIT(con);
              if (m_net_server.connect(epee::string_tools::get_ip_string_from_int32(ipv4.ip()),
                                       epee::string_tools::num_to_string_fast(ipv4.port()),
                                       m_config.m_net_config.connection_timeout, con, m_bind_ip)) {
                  MDEBUG("P2P Request: notify_peer_list: connected to peer: " << pe.adr.host_str()
                               << ", sending command");
                  sent = relay_notify(command, buf, con.m_connection_id);
                  if (!sent)
                    MWARNING("P2P Request: notify_peer_list: peer is connected, sending to : " << pe.adr.host_str() << " FAILED");
              } else {
                  MWARNING("P2P Request: notify_peer_list: failed to connect to peer: " << pe.adr.host_str());
              }
          }
      }
      MDEBUG("P2P Request: notify_peer_list: end notify");
      return true;
  }

  //-----------------------------------------------------------------------------------

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::multicast_send(int command, const std::string &data, const std::list<std::string> &addresses, const std::list<peerid_type> &exclude_peerids)
  {
      MDEBUG("P2P Request: multicast_send: Start tunneling for addresses: "
                   << boost::algorithm::join(addresses, ", "));
      std::vector<peerlist_entry> tunnels;
      {
          std::map<std::string, nodetool::supernode_route> local_supernode_routes;
          {
              MDEBUG("P2P Request: multicast_send: lock");
              boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
              MDEBUG("P2P Request: multicast_send: unlock");
              local_supernode_routes = m_supernode_routes;
          }
          for (auto addr : addresses)
          {
              MDEBUG("P2P Request: multicast_send: looking for tunnel for " << addr);
              auto it = local_supernode_routes.find(addr);
              if (it == local_supernode_routes.end())
              {
                  MWARNING("no tunnel found for address: " << addr);
                  continue;
              }
              // peers for address
              std::vector<peerlist_entry> addr_tunnels = (*it).second.peers;
              unsigned int count = 0;
              for (auto peer_it = addr_tunnels.begin(); peer_it != addr_tunnels.end(); ++peer_it)
              {
                  peerlist_entry addr_tunnel = *peer_it;


                  // check if peer connected connections
                  boost::uuids::uuid dummy;
                  if (!find_connection_id_by_peer(addr_tunnel, dummy))
                    continue;

                  // don't allow duplicate entries
                  auto tunnel_it = std::find_if(tunnels.begin(), tunnels.end(),
                                                [addr_tunnel](const peerlist_entry &entry) -> bool {
                      return entry.id == addr_tunnel.id;
                  });
                  // check if our peer is in excluded peers
                  auto exclude_it = std::find(exclude_peerids.begin(), exclude_peerids.end(),
                                              addr_tunnel.id);
                  if (tunnel_it == tunnels.end() && exclude_it == exclude_peerids.end())
                  {
                      MDEBUG("found tunnel for address: " << addr << ":  " << addr_tunnel.adr.str());
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
      MDEBUG("P2P Request: multicast_send: End tunneling, tunnels found: " << tunnels.size());
      m_multicast_bytes_out += data.size() * tunnels.size();

      return notify_peer_list(command, data, tunnels);
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  uint64_t node_server<t_payload_net_handler>::get_max_hop(const std::list<std::string> &addresses)
  {
      uint64_t max_hop = 0;
      {
          std::map<std::string, nodetool::supernode_route> local_supernode_routes;
          {
              MDEBUG("P2P Request: multicast_send: lock");
              boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
              MDEBUG("P2P Request: multicast_send: unlock");
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
              MDEBUG("P2P Request: multicast_send: lock");
              boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
              MDEBUG("P2P Request: multicast_send: unlock");
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
      MDEBUG("P2P Request: handle_supernode_announce: start");

      m_announce_bytes_in += get_command_size(arg);

      if (context.m_state != p2p_connection_context::state_normal) {
          MWARNING(context << " invalid connection (no handshake)");
          return 1;
      }

#ifdef LOCK_RTA_SENDING
    return 1;
#endif
      static std::string supernode_endpoint("send_supernode_announce");
      std::string supernode_str = arg.supernode_public_id;

      bool is_local;
      {
          boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
          is_local = m_supernodes.count(supernode_str) > 0;
      }
      if (!is_local) {
          MDEBUG("P2P Request: handle_supernode_announce: update tunnels for " << arg.supernode_public_id << " Hop: " << arg.hop << " Address: " << arg.network_address);

          peerlist_entry pe;
          // TODO: Need to investigate it and mechanism for adding peer to the peerlist
          if (!m_peerlist.find_peer(context.peer_id, pe))
          { // unknown peer, alternative handshake with it
              MDEBUG("unknown peer, alternative handshake with it " << context.peer_id);
              return 1;
          }
          {
              MDEBUG("P2P Request: handle_supernode_announce: lock");
              boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
              MDEBUG("P2P Request: handle_supernode_announce: unlock");
              remove_old_request_cache();
          }
          MDEBUG("P2P Request: handle_supernode_announce: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
          MDEBUG("P2P Request: handle_supernode_announce: unlock");

          MDEBUG("P2P Request: handle_supernode_announce: routes number - " << m_supernode_routes.size());
          for (auto it2 = m_supernode_routes.begin(); it2 != m_supernode_routes.end(); ++it2)
          {
              MDEBUG("P2P Request: handle_supernode_announce: " << (*it2).first << " " << (*it2).second.peers.size());
          }


          auto it = m_supernode_routes.find(supernode_str);
          if (it == m_supernode_routes.end())
          {
              std::vector<peerlist_entry> peer_vec;
              peer_vec.push_back(pe);
              nodetool::supernode_route route;
              route.last_announce_height = arg.height;
              route.last_announce_time = time(nullptr);
              route.max_hop = arg.hop;
              route.peers = peer_vec;
              m_supernode_routes[supernode_str] = route;
          }
          else {
              auto &route = it->second;
#if 0
              // this check is not correct in case stake transactions if older tx has greater lock time than newer one
              if (route.last_announce_height > arg.height)
              {
                  MINFO("SUPERNODE_ANNOUNCE from " << context.peer_id
                        << " too old, corrent route height " << (*it).second.last_announce_height);
                  return 1;
              }
#endif

              if (route.last_announce_height == arg.height && route.last_announce_time + DIFFICULTY_TARGET_V2 > (unsigned)time(nullptr))
              {
                  MDEBUG("existing announce, height: " << arg.height << ", last_announce_time: " << (*it).second.last_announce_time
                         << ", current time: " << time(nullptr));
                  auto peer_it = std::find_if(route.peers.begin(), route.peers.end(),
                                              [pe](const peerlist_entry &p) -> bool { return pe.id == p.id; });
                  if (peer_it == route.peers.end())
                  {
                      route.peers.push_back(pe);
                      if (route.max_hop < arg.hop)
                      {
                          route.max_hop = arg.hop;
                      }
                  }
                  return 1;
              }
              route.peers.clear();
              route.peers.push_back(pe);
              route.last_announce_height = arg.height;
              route.last_announce_time = time(nullptr);
              route.max_hop = arg.hop;
          }
      }

      std::list<std::reference_wrapper<local_supernode>> post_to_sn;
      {
          LOG_PRINT_L3("P2P Request: handle_supernode_announce: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
          LOG_PRINT_L3("P2P Request: handle_supernode_announce: unlock");
          for (auto &sn : m_supernodes) {
              if (sn.first != supernode_str)
                  post_to_sn.push_back(std::ref(sn.second));
          }
      }
      for (auto &sn : post_to_sn) {
          LOG_PRINT_L1("P2P Request: handle_supernode_announce: post to supernode");
          post_request_to_supernode<cryptonote::COMMAND_RPC_SUPERNODE_ANNOUNCE>(sn, supernode_endpoint, arg);
      }

      if (!is_local) {
          // Notify neighbours about new ANNOUNCE
          arg.hop++;
          MDEBUG("P2P Request: handle_supernode_announce: notify peers " << arg.hop);

          std::list<boost::uuids::uuid> all_connections, random_connections;
          m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
          {
            // skip ourself connections
            if(cntxt.peer_id == m_config.m_peer_id)
              return true;
            all_connections.push_back(cntxt.m_connection_id);
            return true;
          });

          if (all_connections.empty()) {
            MWARNING("P2P Request: no connections to relay announce");
            return 1;
          }

          select_subset_with_probability(1.0 / all_connections.size(), all_connections, random_connections);

          std::string arg_buff;
          epee::serialization::store_t_to_binary(arg, arg_buff);

          MDEBUG("P2P Request: handle_supernode_announce: relaying to neighbours: " << random_connections.size());

          relay_notify_to_list(command, arg_buff, random_connections);
          m_announce_bytes_out += arg_buff.size() * random_connections.size();
      }

      MDEBUG("P2P Request: handle_supernode_announce: end");
      return 1;
  }

  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_broadcast(int command, typename COMMAND_BROADCAST::request &arg, p2p_connection_context &context)
  {
      MDEBUG("P2P Request: handle_broadcast: start");

      m_broadcast_bytes_in += get_command_size(arg);

      if (context.m_state != p2p_connection_context::state_normal) {
          MWARNING(context << " invalid connection (no handshake)");
          return 1;
      }

#ifdef LOCK_RTA_SENDING
    return 1;
#endif

      {
          MDEBUG("P2P Request: handle_broadcast: lock");
          boost::unique_lock<boost::recursive_mutex> cache_guard(m_request_cache_lock, boost::defer_lock),
              sn_guard(m_supernode_lock, boost::defer_lock);
          boost::lock(cache_guard, sn_guard);
          MDEBUG("P2P Request: handle_broadcast: unlock");
          MDEBUG("P2P Request: handle_broadcast: sender_address: " << arg.sender_address
                       << ", our address(es): " << join_supernodes_addresses(", "));
          if (m_supernode_requests_cache.find(arg.message_id) == m_supernode_requests_cache.end())
          {
              MDEBUG("P2P Request: handle_broadcast: post to supernodes");
              m_supernode_requests_cache.insert(arg.message_id);
              int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
              m_supernode_requests_timestamps.insert(std::make_pair(timestamp, arg.message_id));

              post_request_to_supernodes<cryptonote::COMMAND_RPC_BROADCAST>("broadcast", arg, arg.callback_uri);

              if (arg.hop > 0)
              {
                  MDEBUG("P2P Request: handle_broadcast: notify broadcast from " << arg.sender_address
                               << " to peers. Hop level: " << arg.hop);
                  arg.hop--;
                  std::string buff;
                  epee::serialization::store_t_to_binary(arg, buff);

                  m_broadcast_bytes_out += buff.size() * get_connections_count();

                  relay_notify_to_all(command, buff, context);
              }
              else
              {
                  MDEBUG("P2P Request: handle_broadcast: hop counter ended for broadcast from "
                               << arg.sender_address);
              }
          }
          MDEBUG("P2P Request: handle_broadcast: clean request cache");
          remove_old_request_cache();
      }
      MDEBUG("P2P Request: handle_broadcast: end");
      return 1;
  }

  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_multicast(int command, typename COMMAND_MULTICAST::request &arg, p2p_connection_context &context)
  {
      MDEBUG("P2P Request: handle_multicast: start");

      m_multicast_bytes_in += get_command_size(arg);

      if (context.m_state != p2p_connection_context::state_normal) {
          MWARNING(context << " invalid connection (no handshake)");
          return 1;
      }

#ifdef LOCK_RTA_SENDING
    return 1;
#endif

      std::list<std::string> addresses = arg.receiver_addresses;
      bool forward = false;
      {
          MDEBUG("P2P Request: handle_multicast: lock");
          boost::unique_lock<boost::recursive_mutex> cache_guard(m_request_cache_lock, boost::defer_lock),
              sn_guard(m_supernode_lock, boost::defer_lock);
          boost::lock(cache_guard, sn_guard);

          MDEBUG("P2P Request: handle_multicast: unlock");
          MDEBUG("P2P Request: handle_multicast: sender_address: " << arg.sender_address
                       << ", receiver_addresses: " << boost::algorithm::join(arg.receiver_addresses, ", ")
                       << ", our address(es): " << join_supernodes_addresses(", "));
          if (m_supernode_requests_cache.find(arg.message_id) == m_supernode_requests_cache.end())
          {
              MDEBUG("P2P Request: handle_multicast: post to supernodes");
              m_supernode_requests_cache.insert(arg.message_id);
              int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
              m_supernode_requests_timestamps.insert(std::make_pair(timestamp, arg.message_id));
              for (auto it = addresses.begin(); it != addresses.end(); ) {
                  auto snit = m_supernodes.find(*it);
                  if (snit != m_supernodes.end()) {
                      MDEBUG("P2P Request: handle_multicast: posting to local supernode " << snit->first);
                      post_request_to_supernode<cryptonote::COMMAND_RPC_MULTICAST>(snit->second, "multicast", arg, arg.callback_uri);
                      it = addresses.erase(it);
                  } else {
                      ++it;
                  }
              }

              if (arg.hop > 0)
              {
                  forward = true;
              }
              else
              {
                  MDEBUG("P2P Request: handle_multicast: hop counter ended for multicast from "
                               << arg.sender_address);
              }
          }
          else
          {
              MDEBUG("P2P Request: handle_multicast: request found in cache, skipping");
          }
          MDEBUG("P2P Request: handle_multicast: clean request cache");
          remove_old_request_cache();
      }
      if (forward)
      {
          MDEBUG("P2P Request: handle_multicast: notify multicast from " << arg.sender_address
                       << " to peers. Hop level: " << arg.hop);
          arg.hop--;
          std::list<peerid_type> exclude_peers;
          exclude_peers.push_back(context.peer_id);

          std::string buff;
          epee::serialization::store_t_to_binary(arg, buff);
          multicast_send(command, buff, addresses, exclude_peers);
      }
      MDEBUG("P2P Request: handle_multicast: end");
      return 1;
  }

  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_unicast(int command, typename COMMAND_UNICAST::request &arg, p2p_connection_context &context)
  {
      MDEBUG("P2P Request: handle_unicast: start");
      m_multicast_bytes_in += get_command_size(arg);
      if (context.m_state != p2p_connection_context::state_normal) {
          MWARNING(context << " invalid connection (no handshake)");
          return 1;
      }

#ifdef LOCK_RTA_SENDING
    return 1;
#endif

      std::string address = arg.receiver_address;
      bool forward = false;
      {
          MDEBUG("P2P Request: handle_unicast: lock");
          boost::unique_lock<boost::recursive_mutex> cache_guard(m_request_cache_lock, boost::defer_lock),
              sn_guard(m_supernode_lock, boost::defer_lock);
          boost::lock(cache_guard, sn_guard);
          MDEBUG("P2P Request: handle_unicast: unlock");
          MDEBUG("P2P Request: handle_unicast: sender_address: " << arg.sender_address
                       << ", receiver_address: " << arg.receiver_address
                       << ", our address(es): " << join_supernodes_addresses(", "));
          if (m_supernode_requests_cache.find(arg.message_id) == m_supernode_requests_cache.end())
          {
              MDEBUG("P2P Request: handle_unicast: post to supernodes");
              m_supernode_requests_cache.insert(arg.message_id);
              int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
              m_supernode_requests_timestamps.insert(std::make_pair(timestamp, arg.message_id));
              auto it = m_supernodes.find(address);
              bool local_sn = it != m_supernodes.end();
              if (local_sn) {
                  MDEBUG("P2P Request: handle_unicast: sending to local supernode " << address);
                  post_request_to_supernode<cryptonote::COMMAND_RPC_UNICAST>(it->second, "unicast", arg, arg.callback_uri);
              }
              else if (arg.hop > 0)
              {
                  forward = true;
              }
              else
              {
                  MDEBUG("P2P Request: handle_unicast: hop counter ended for unicast from "
                               << arg.sender_address);
              }
          }
          else
          {
              MDEBUG("P2P Request: handle_unicast: request found in cache, skipping");
          }
          MDEBUG("P2P Request: handle_unicast: clean request cache");
          remove_old_request_cache();
      }

      if (forward)
      {
          MDEBUG("P2P Request: handle_unicast: notify unicast from " << arg.sender_address
                       << " to " << arg.receiver_address << ". Hop level: " << arg.hop);
          arg.hop--;
          std::list<std::string> addresses;
          addresses.push_back(address);

          std::list<peerid_type> exclude_peers;
          exclude_peers.push_back(context.peer_id);

          std::string buff;
          epee::serialization::store_t_to_binary(arg, buff);
          multicast_send(command, buff, addresses, exclude_peers);
      }
      MDEBUG("P2P Request: handle_unicast: end");
      return 1;
  }
#endif
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::do_handshake_with_peer(peerid_type& pi, p2p_connection_context& context_, bool just_take_peerlist)
  {
    network_zone& zone = m_network_zones.at(context_.m_remote_address.get_zone());

    typename COMMAND_HANDSHAKE::request arg;
    typename COMMAND_HANDSHAKE::response rsp;
    get_local_node_data(arg.node_data, zone);
    m_payload_handler.get_payload_sync_data(arg.payload_data);

    epee::simple_event ev;
    std::atomic<bool> hsh_result(false);

    bool r = epee::net_utils::async_invoke_remote_command2<typename COMMAND_HANDSHAKE::response>(context_.m_connection_id, COMMAND_HANDSHAKE::ID, arg, zone.m_net_server.get_config_object(),
      [this, &pi, &ev, &hsh_result, &just_take_peerlist, &context_](int code, typename COMMAND_HANDSHAKE::response&& rsp, p2p_connection_context& context)
    {
      epee::misc_utils::auto_scope_leave_caller scope_exit_handler = epee::misc_utils::create_scope_leave_handler([&](){ev.raise();});

      if(code < 0)
      {
        LOG_WARNING_CC(context, "COMMAND_HANDSHAKE invoke failed. (" << code <<  ", " << epee::levin::get_err_descr(code) << ")");
        return;
      }

      if(rsp.node_data.network_id != m_network_id)
      {
        LOG_WARNING_CC(context, "COMMAND_HANDSHAKE Failed, wrong network!  (" << rsp.node_data.network_id << "), closing connection.");
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
        if(!m_payload_handler.process_payload_sync_data(std::move(rsp.payload_data), context, true))
        {
          LOG_WARNING_CC(context, "COMMAND_HANDSHAKE invoked, but process_payload_sync_data returned false, dropping connection.");
          hsh_result = false;
          return;
        }

        pi = context.peer_id = rsp.node_data.peer_id;
        context.m_rpc_port = rsp.node_data.rpc_port;
        m_network_zones.at(context.m_remote_address.get_zone()).m_peerlist.set_peer_just_seen(rsp.node_data.peer_id, context.m_remote_address, context.m_pruning_seed, context.m_rpc_port);

        // move
        for (auto const& zone : m_network_zones)
        {
          if(rsp.node_data.peer_id == zone.second.m_config.m_peer_id)
          {
            LOG_DEBUG_CC(context, "Connection to self detected, dropping connection");
            hsh_result = false;
            return;
          }
        }
        LOG_INFO_CC(context, "New connection handshaked, pruning seed " << epee::string_tools::to_string_hex(context.m_pruning_seed));
        LOG_DEBUG_CC(context, " COMMAND_HANDSHAKE INVOKED OK");
      }else
      {
        LOG_DEBUG_CC(context, " COMMAND_HANDSHAKE(AND CLOSE) INVOKED OK");
      }
      context_ = context;
    }, P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT);

    if(r)
    {
      ev.wait();
    }

    if(!hsh_result)
    {
      LOG_WARNING_CC(context_, "COMMAND_HANDSHAKE Failed");
      m_network_zones.at(context_.m_remote_address.get_zone()).m_net_server.get_config_object().close(context_.m_connection_id);
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
    typename COMMAND_TIMED_SYNC::request arg{};
    m_payload_handler.get_payload_sync_data(arg.payload_data);

    network_zone& zone = m_network_zones.at(context_.m_remote_address.get_zone());
    bool r = epee::net_utils::async_invoke_remote_command2<typename COMMAND_TIMED_SYNC::response>(context_.m_connection_id, COMMAND_TIMED_SYNC::ID, arg, zone.m_net_server.get_config_object(),
      [this](int code, typename COMMAND_TIMED_SYNC::response&& rsp, p2p_connection_context& context)
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
        m_network_zones.at(context.m_remote_address.get_zone()).m_net_server.get_config_object().close(context.m_connection_id );
        add_host_fail(context.m_remote_address);
      }
      if(!context.m_is_income)
        m_network_zones.at(context.m_remote_address.get_zone()).m_peerlist.set_peer_just_seen(context.peer_id, context.m_remote_address, context.m_pruning_seed, context.m_rpc_port);
      if (!m_payload_handler.process_payload_sync_data(std::move(rsp.payload_data), context, false))
      {
        m_network_zones.at(context.m_remote_address.get_zone()).m_net_server.get_config_object().close(context.m_connection_id );
      }
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
    for(const auto& zone : m_network_zones)
      if(zone.second.m_config.m_peer_id == peer.id)
        return true;//dont make connections to ourself

    bool used = false;
    for(auto& zone : m_network_zones)
    {
      zone.second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
      {
        if(cntxt.peer_id == peer.id || (!cntxt.m_is_income && peer.adr == cntxt.m_remote_address))
        {
          used = true;
          return false;//stop enumerating
        }
        return true;
      });

      if(used)
        return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_peer_used(const anchor_peerlist_entry& peer)
  {
    for(auto& zone : m_network_zones) {
      if(zone.second.m_config.m_peer_id == peer.id) {
          return true;//dont make connections to ourself
      }
      bool used = false;
      zone.second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
      {
        if(cntxt.peer_id == peer.id || (!cntxt.m_is_income && peer.adr == cntxt.m_remote_address))
        {
          used = true;
          return false;//stop enumerating
        }
        return true;
      });
      if (used)
        return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_addr_connected(const epee::net_utils::network_address& peer)
  {
    const auto zone = m_network_zones.find(peer.get_zone());
    if (zone == m_network_zones.end())
      return false;

    bool connected = false;
    zone->second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
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
    network_zone& zone = m_network_zones.at(na.get_zone());
    if (zone.m_connect == nullptr) // outgoing connections in zone not possible
      return false;

    if (zone.m_current_number_of_out_peers == zone.m_config.m_net_config.max_out_connection_count) // out peers limit
    {
      return false;
    }
    else if (zone.m_current_number_of_out_peers > zone.m_config.m_net_config.max_out_connection_count)
    {
      zone.m_net_server.get_config_object().del_out_connections(1);
      --(zone.m_current_number_of_out_peers); // atomic variable, update time = 1s
      return false;
    }


    MDEBUG("Connecting to " << na.str() << "(peer_type=" << peer_type << ", last_seen: "
        << (last_seen_stamp ? epee::misc_utils::get_time_interval_string(time(NULL) - last_seen_stamp):"never")
        << ")...");

    auto con = zone.m_connect(zone, na, m_ssl_support);
    if(!con)
    {
      bool is_priority = is_priority_node(na);
      LOG_PRINT_CC_PRIORITY_NODE(is_priority, bool(con), "Connect failed to " << na.str()
        /*<< ", try " << try_count*/);
      //m_peerlist.set_peer_unreachable(pe);
      return false;
    }

    con->m_anchor = peer_type == anchor;
    peerid_type pi{};
    bool res = do_handshake_with_peer(pi, *con, just_take_peerlist);

    if(!res)
    {
      bool is_priority = is_priority_node(na);
      LOG_PRINT_CC_PRIORITY_NODE(is_priority, *con, "Failed to HANDSHAKE with peer "
        << na.str()
        /*<< ", try " << try_count*/);
      zone.m_net_server.get_config_object().close(con->m_connection_id);
      return false;
    }

    if(just_take_peerlist)
    {
      zone.m_net_server.get_config_object().close(con->m_connection_id);
      LOG_DEBUG_CC(*con, "CONNECTION HANDSHAKED OK AND CLOSED.");
      return true;
    }

    peerlist_entry pe_local{};
    pe_local.adr = na;
    pe_local.id = pi;
    time_t last_seen;
    time(&last_seen);
    pe_local.last_seen = static_cast<int64_t>(last_seen);
    pe_local.pruning_seed = con->m_pruning_seed;
    pe_local.rpc_port = con->m_rpc_port;
    zone.m_peerlist.append_with_peer_white(pe_local);
    //update last seen and push it to peerlist manager

    anchor_peerlist_entry ape{};
    ape.adr = na;
    ape.id = pi;
    ape.first_seen = first_seen_stamp ? first_seen_stamp : time(nullptr);

    zone.m_peerlist.append_with_peer_anchor(ape);

    LOG_DEBUG_CC(*con, "CONNECTION HANDSHAKED OK.");
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::check_connection_and_handshake_with_peer(const epee::net_utils::network_address& na, uint64_t last_seen_stamp)
  {
    network_zone& zone = m_network_zones.at(na.get_zone());
    if (zone.m_connect == nullptr)
      return false;

    LOG_PRINT_L1("Connecting to " << na.str() << "(last_seen: "
                                  << (last_seen_stamp ? epee::misc_utils::get_time_interval_string(time(NULL) - last_seen_stamp):"never")
                                  << ")...");

    auto con = zone.m_connect(zone, na, m_ssl_support);
    if (!con) {
      bool is_priority = is_priority_node(na);

      LOG_PRINT_CC_PRIORITY_NODE(is_priority, p2p_connection_context{}, "Connect failed to " << na.str());

      return false;
    }

    con->m_anchor = false;
    peerid_type pi{};
    const bool res = do_handshake_with_peer(pi, *con, true);
    if (!res) {
      bool is_priority = is_priority_node(na);

      LOG_PRINT_CC_PRIORITY_NODE(is_priority, *con, "Failed to HANDSHAKE with peer " << na.str());
      zone.m_net_server.get_config_object().close(con->m_connection_id);
      return false;
    }

    zone.m_net_server.get_config_object().close(con->m_connection_id);

    LOG_DEBUG_CC(*con, "CONNECTION HANDSHAKED OK AND CLOSED.");

    return true;
  }

#undef LOG_PRINT_CC_PRIORITY_NODE

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::is_addr_recently_failed(const epee::net_utils::network_address& addr)
  {
    CRITICAL_REGION_LOCAL(m_conn_fails_cache_lock);
    auto it = m_conn_fails_cache.find(addr.host_str());
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
      _note("Considering connecting (out) to anchor peer: " << peerid_type(pe.id) << " " << pe.adr.str());

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
  bool node_server<t_payload_net_handler>::make_new_connection_from_peerlist(network_zone& zone, bool use_white_list)
  {
    size_t max_random_index = 0;

    std::set<size_t> tried_peers;

    size_t try_count = 0;
    size_t rand_count = 0;
    while(rand_count < (max_random_index+1)*3 &&  try_count < 10 && !zone.m_net_server.is_stop_signal_sent())
    {
      ++rand_count;
      size_t random_index;
      const uint32_t next_needed_pruning_stripe = m_payload_handler.get_next_needed_pruning_stripe().second;

      // build a set of all the /16 we're connected to, and prefer a peer that's not in that set
      std::set<uint32_t> classB;
      if (&zone == &m_network_zones.at(epee::net_utils::zone::public_)) // at returns reference, not copy
      {
        zone.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
        {
          if (cntxt.m_remote_address.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id())
          {

            const epee::net_utils::network_address na = cntxt.m_remote_address;
            const uint32_t actual_ip = na.as<const epee::net_utils::ipv4_network_address>().ip();
            classB.insert(actual_ip & 0x0000ffff);
          }
          return true;
        });
      }

      std::deque<size_t> filtered;
      const size_t limit = use_white_list ? 20 : std::numeric_limits<size_t>::max();
      size_t idx = 0, skipped = 0;
      for (int step = 0; step < 2; ++step)
      {
        bool skip_duplicate_class_B = step == 0 && m_nettype == cryptonote::MAINNET;
        zone.m_peerlist.foreach (use_white_list, [&classB, &filtered, &idx, &skipped, skip_duplicate_class_B, limit, next_needed_pruning_stripe](const peerlist_entry &pe){
          if (filtered.size() >= limit)
            return false;
          bool skip = false;
          if (skip_duplicate_class_B && pe.adr.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id())
          {
            const epee::net_utils::network_address na = pe.adr;
            uint32_t actual_ip = na.as<const epee::net_utils::ipv4_network_address>().ip();
            skip = classB.find(actual_ip & 0x0000ffff) != classB.end();
          }
          if (skip)
            ++skipped;
          else if (next_needed_pruning_stripe == 0 || pe.pruning_seed == 0)
            filtered.push_back(idx);
          else if (next_needed_pruning_stripe == tools::get_pruning_stripe(pe.pruning_seed))
            filtered.push_front(idx);
          ++idx;
          return true;
        });
        if (skipped == 0 || !filtered.empty())
          break;
        if (skipped)
          MGINFO("Skipping " << skipped << " possible peers as they share a class B with existing peers");
      }
      if (filtered.empty())
      {
        MDEBUG("No available peer in " << (use_white_list ? "white" : "gray") << " list filtered by " << next_needed_pruning_stripe);
        return false;
      }
      if (use_white_list)
      {
        // if using the white list, we first pick in the set of peers we've already been using earlier
        random_index = get_random_index_with_fixed_probability(std::min<uint64_t>(filtered.size() - 1, 20));
        CRITICAL_REGION_LOCAL(m_used_stripe_peers_mutex);
        if (next_needed_pruning_stripe > 0 && next_needed_pruning_stripe <= (1ul << CRYPTONOTE_PRUNING_LOG_STRIPES) && !m_used_stripe_peers[next_needed_pruning_stripe-1].empty())
        {
          const epee::net_utils::network_address na = m_used_stripe_peers[next_needed_pruning_stripe-1].front();
          m_used_stripe_peers[next_needed_pruning_stripe-1].pop_front();
          for (size_t i = 0; i < filtered.size(); ++i)
          {
            peerlist_entry pe;
            if (zone.m_peerlist.get_white_peer_by_index(pe, filtered[i]) && pe.adr == na)
            {
              MDEBUG("Reusing stripe " << next_needed_pruning_stripe << " peer " << pe.adr.str());
              random_index = i;
              break;
            }
          }
        }
      }
      else
        random_index = crypto::rand_idx(filtered.size());

      CHECK_AND_ASSERT_MES(random_index < filtered.size(), false, "random_index < filtered.size() failed!!");
      random_index = filtered[random_index];
      CHECK_AND_ASSERT_MES(random_index < (use_white_list ? zone.m_peerlist.get_white_peers_count() : zone.m_peerlist.get_gray_peers_count()),
          false, "random_index < peers size failed!!");

      if(tried_peers.count(random_index))
        continue;

      tried_peers.insert(random_index);
      peerlist_entry pe{};
      bool r = use_white_list ? zone.m_peerlist.get_white_peer_by_index(pe, random_index):zone.m_peerlist.get_gray_peer_by_index(pe, random_index);
      CHECK_AND_ASSERT_MES(r, false, "Failed to get random peer from peerlist(white:" << use_white_list << ")");

      ++try_count;

      _note("Considering connecting (out) to " << (use_white_list ? "white" : "gray") << " list peer: " <<
          peerid_to_string(pe.id) << " " << pe.adr.str() << ", pruning seed " << epee::string_tools::to_string_hex(pe.pruning_seed) <<
          " (stripe " << next_needed_pruning_stripe << " needed)");

      if(is_peer_used(pe)) {
        _note("Peer is used");
        continue;
      }

      if(!is_remote_host_allowed(pe.adr))
        continue;

      if(is_addr_recently_failed(pe.adr))
        continue;

      MDEBUG("Selected peer: " << peerid_to_string(pe.id) << " " << pe.adr.str()
                    << ", pruning seed " << epee::string_tools::to_string_hex(pe.pruning_seed) << " "
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
#if 0 // TODO: Graft: remove
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::find_connection_id_by_peer(const peerlist_entry &pe, boost::uuids::uuid& conn_id)
  {
    bool ret = false;
    MDEBUG("find_connection_id_by_peer: looking for: " << pe.adr.str());
    m_net_server.get_config_object().foreach_connection([&pe, &ret, &conn_id](p2p_connection_context& cntxt)
    {
      if (cntxt.peer_id == pe.id) {
        conn_id = cntxt.m_connection_id;
        ret = true;
        return false; // found connection, stopping foreach_connection loop
      }
      return true;
    });
    MDEBUG("find_connection_id_by_peer: done looking for: " << pe.adr.str() << ", found: " << conn_id);
    return ret;
  }
#endif  


  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::connect_to_seed()
  {
      if (m_seed_nodes.empty() || m_offline || !m_exclusive_peers.empty())
        return true;

      size_t try_count = 0;
      size_t current_index = crypto::rand_idx(m_seed_nodes.size());
      const net_server& server = m_network_zones.at(epee::net_utils::zone::public_).m_net_server;
      while(true)
      {
        if(server.is_stop_signal_sent())
          return false;

        if(try_to_connect_and_handshake_with_new_peer(m_seed_nodes[current_index], true))
          break;
        if(++try_count > m_seed_nodes.size())
        {
          if (!m_fallback_seed_nodes_added)
          {
            MWARNING("Failed to connect to any of seed peers, trying fallback seeds");
            current_index = m_seed_nodes.size();
            for (const auto &peer: get_seed_nodes(m_nettype))
            {
              MDEBUG("Fallback seed node: " << peer);
              append_net_address(m_seed_nodes, peer, cryptonote::get_config(m_nettype).P2P_DEFAULT_PORT);
            }
            m_fallback_seed_nodes_added = true;
            if (current_index == m_seed_nodes.size())
            {
              MWARNING("No fallback seeds, continuing without seeds");
              break;
            }
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
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::connections_maker()
  {
    using zone_type = epee::net_utils::zone;

    if (m_offline) return true;
    if (!connect_to_peerlist(m_exclusive_peers)) return false;

    if (!m_exclusive_peers.empty()) return true;

    // Only have seeds in the public zone right now.

    size_t start_conn_count = get_public_outgoing_connections_count();
    if(!get_public_white_peers_count() && m_seed_nodes.size())
    {
      if (!connect_to_seed())
        return false;
    }

    if (!connect_to_peerlist(m_priority_peers)) return false;

    for(auto& zone : m_network_zones)
    {
      size_t base_expected_white_connections = (zone.second.m_config.m_net_config.max_out_connection_count*P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT)/100;

      size_t conn_count = get_outgoing_connections_count(zone.second);
      while(conn_count < zone.second.m_config.m_net_config.max_out_connection_count)
      {
        const size_t expected_white_connections = m_payload_handler.get_next_needed_pruning_stripe().second ? zone.second.m_config.m_net_config.max_out_connection_count : base_expected_white_connections;
        if(conn_count < expected_white_connections)
        {
          //start from anchor list
          while (get_outgoing_connections_count(zone.second) < P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT
            && make_expected_connections_count(zone.second, anchor, P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT));
          //then do white list
          while (get_outgoing_connections_count(zone.second) < expected_white_connections
            && make_expected_connections_count(zone.second, white, expected_white_connections));
          //then do grey list
          while (get_outgoing_connections_count(zone.second) < zone.second.m_config.m_net_config.max_out_connection_count
            && make_expected_connections_count(zone.second, gray, zone.second.m_config.m_net_config.max_out_connection_count));
        }else
        {
          //start from grey list
          while (get_outgoing_connections_count(zone.second) < zone.second.m_config.m_net_config.max_out_connection_count
            && make_expected_connections_count(zone.second, gray, zone.second.m_config.m_net_config.max_out_connection_count));
          //and then do white list
          while (get_outgoing_connections_count(zone.second) < zone.second.m_config.m_net_config.max_out_connection_count
            && make_expected_connections_count(zone.second, white, zone.second.m_config.m_net_config.max_out_connection_count));
        }
        if(zone.second.m_net_server.is_stop_signal_sent())
          return false;
        size_t new_conn_count = get_outgoing_connections_count(zone.second);
        if (new_conn_count <= conn_count)
        {
          // we did not make any connection, sleep a bit to avoid a busy loop in case we don't have
          // any peers to try, then break so we will try seeds to get more peers
          boost::this_thread::sleep_for(boost::chrono::seconds(1));
          break;
        }
        conn_count = new_conn_count;
      }
    }

    if (start_conn_count == get_public_outgoing_connections_count() && start_conn_count < m_network_zones.at(zone_type::public_).m_config.m_net_config.max_out_connection_count)
    {
      MINFO("Failed to connect to any, trying seeds");
      if (!connect_to_seed())
        return false;
    }

    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::make_expected_connections_count(network_zone& zone, PeerType peer_type, size_t expected_connections)
  {
    if (m_offline)
      return false;

    std::vector<anchor_peerlist_entry> apl;

    if (peer_type == anchor) {
      zone.m_peerlist.get_and_empty_anchor_peerlist(apl);
    }

    size_t conn_count = get_outgoing_connections_count(zone);
    //add new connections from white peers
    if(conn_count < expected_connections)
    {
      if(zone.m_net_server.is_stop_signal_sent())
        return false;

      MDEBUG("Making expected connection, type " << peer_type << ", " << conn_count << "/" << expected_connections << " connections");

      if (peer_type == anchor && !make_new_connection_from_anchor_peerlist(apl)) {
        return false;
      }

      if (peer_type == white && !make_new_connection_from_peerlist(zone, true)) {
        return false;
      }

      if (peer_type == gray && !make_new_connection_from_peerlist(zone, false)) {
        return false;
      }
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_public_outgoing_connections_count()
  {
    auto public_zone = m_network_zones.find(epee::net_utils::zone::public_);
    if (public_zone == m_network_zones.end())
      return 0;
    return get_outgoing_connections_count(public_zone->second);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_incoming_connections_count(network_zone& zone)
  {
    size_t count = 0;
    zone.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(cntxt.m_is_income)
        ++count;
      return true;
    });
    return count;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_outgoing_connections_count(network_zone& zone)
  {
    size_t count = 0;
    zone.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      if(!cntxt.m_is_income)
        ++count;
      return true;
    });
    return count;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_outgoing_connections_count()
  {
    size_t count = 0;
    for(auto& zone : m_network_zones)
      count += get_outgoing_connections_count(zone.second);
    return count;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_incoming_connections_count()
  {
    size_t count = 0;
    for (auto& zone : m_network_zones)
    {
      zone.second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
      {
        if(cntxt.m_is_income)
          ++count;
        return true;
      });
    }
    return count;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_public_white_peers_count()
  {
    auto public_zone = m_network_zones.find(epee::net_utils::zone::public_);
    if (public_zone == m_network_zones.end())
      return 0;
    return public_zone->second.m_peerlist.get_white_peers_count();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  size_t node_server<t_payload_net_handler>::get_public_gray_peers_count()
  {
    auto public_zone = m_network_zones.find(epee::net_utils::zone::public_);
    if (public_zone == m_network_zones.end())
      return 0;
    return public_zone->second.m_peerlist.get_gray_peers_count();
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::get_public_peerlist(std::vector<peerlist_entry>& gray, std::vector<peerlist_entry>& white)
  {
    auto public_zone = m_network_zones.find(epee::net_utils::zone::public_);
    if (public_zone != m_network_zones.end())
      public_zone->second.m_peerlist.get_peerlist(gray, white);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::idle_worker()
  {
    m_peer_handshake_idle_maker_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::peer_sync_idle_maker, this));
    m_connections_maker_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::connections_maker, this));
    m_gray_peerlist_housekeeping_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::gray_peerlist_housekeeping, this));
    m_peerlist_store_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::store_config, this));
    m_incoming_connections_interval.do_call(boost::bind(&node_server<t_payload_net_handler>::check_incoming_connections, this));
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::check_incoming_connections()
  {
    if (m_offline)
      return true;

    const auto public_zone = m_network_zones.find(epee::net_utils::zone::public_);
    if (public_zone != m_network_zones.end() && get_incoming_connections_count(public_zone->second) == 0)
    {
      if (m_hide_my_port || public_zone->second.m_config.m_net_config.max_in_connection_count == 0)
      {
        MGINFO("Incoming connections disabled, enable them for full connectivity");
      }
      else
      {
        if (m_igd == delayed_igd)
        {
          MWARNING("No incoming connections, trying to setup IGD");
          add_upnp_port_mapping(m_listening_port);
          m_igd = igd;
        }
        else
        {
          const el::Level level = el::Level::Warning;
          MCLOG_RED(level, "global", "No incoming connections - check firewalls/routers allow port " << get_this_peer_port());
        }
      }
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::peer_sync_idle_maker()
  {
    // TODO: this sync code is rather dumb: every 60s we trigger a sync with every connected peer
    // all at once which results in a sudden spike of activity every 60s then not much in between.
    // This really should be spaced out, i.e. the 60s sync timing should apply per peer, not
    // globally.

    MDEBUG("STARTED PEERLIST IDLE HANDSHAKE");
    typedef std::list<std::pair<epee::net_utils::connection_context_base, peerid_type> > local_connects_type;
    local_connects_type cncts;
    for(auto& zone : m_network_zones)
    {
      zone.second.m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& cntxt)
      {
        if(cntxt.peer_id && !cntxt.m_in_timedsync)
        {
          cntxt.m_in_timedsync = true;
          cncts.push_back(local_connects_type::value_type(cntxt, cntxt.peer_id));//do idle sync only with handshaked connections
        }
        return true;
      });
    }

    std::for_each(cncts.begin(), cncts.end(), [&](const typename local_connects_type::value_type& vl){do_peer_timed_sync(vl.first, vl.second);});

    MDEBUG("FINISHED PEERLIST IDLE HANDSHAKE");
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::fix_time_delta(std::vector<peerlist_entry>& local_peerlist, time_t local_time, int64_t& delta)
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
#ifdef CRYPTONOTE_PRUNING_DEBUG_SPOOF_SEED
      be.pruning_seed = tools::make_pruning_seed(1 + (be.adr.as<epee::net_utils::ipv4_network_address>().ip()) % (1ul << CRYPTONOTE_PRUNING_LOG_STRIPES), CRYPTONOTE_PRUNING_LOG_STRIPES);
#endif
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::handle_remote_peerlist(const std::vector<peerlist_entry>& peerlist, time_t local_time, const epee::net_utils::connection_context_base& context)
  {
    int64_t delta = 0;
    std::vector<peerlist_entry> peerlist_ = peerlist;
    if(!fix_time_delta(peerlist_, local_time, delta))
      return false;

    const epee::net_utils::zone zone = context.m_remote_address.get_zone();
    for(const auto& peer : peerlist_)
    {
      if(peer.adr.get_zone() != zone)
      {
        MWARNING(context << " sent peerlist from another zone, dropping");
        return false;
      }
    }

    LOG_DEBUG_CC(context, "REMOTE PEERLIST: TIME_DELTA: " << delta << ", remote peerlist size=" << peerlist_.size());
    LOG_DEBUG_CC(context, "REMOTE PEERLIST: " <<  print_peerlist_to_string(peerlist_));
    return m_network_zones.at(context.m_remote_address.get_zone()).m_peerlist.merge_peerlist(peerlist_);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::get_local_node_data(basic_node_data& node_data, const network_zone& zone)
  {
    time_t local_time;
    time(&local_time);
    node_data.local_time = local_time; // \TODO This can be an identifying value across zones (public internet to tor/i2p) ...
    node_data.peer_id = zone.m_config.m_peer_id;
    if(!m_hide_my_port && zone.m_can_pingback)
      node_data.my_port = m_external_port ? m_external_port : m_listening_port;
    else
      node_data.my_port = 0;
    node_data.rpc_port = zone.m_can_pingback ? m_rpc_port : 0;
    node_data.network_id = m_network_id;
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::check_trust(const proof_of_trust& tr, const epee::net_utils::zone zone_type)
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

    const network_zone& zone = m_network_zones.at(zone_type);
    if(zone.m_config.m_peer_id != tr.peer_id)
    {
      MWARNING("check_trust failed: peer_id mismatch (passed " << tr.peer_id << ", expected " << zone.m_config.m_peer_id<< ")");
      return false;
    }
    crypto::public_key pk{};
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
    if(!check_trust(arg.tr, context.m_remote_address.get_zone()))
    {
      drop_connection(context);
      return 1;
    }
    rsp.connections_count = get_connections_count();
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
    if(!check_trust(arg.tr, context.m_remote_address.get_zone()))
    {
      drop_connection(context);
      return 1;
    }
    m_network_zones.at(epee::net_utils::zone::public_).m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
    {
      connection_entry ce;
      ce.adr  = cntxt.m_remote_address;
      ce.id = cntxt.peer_id;
      ce.is_income = cntxt.m_is_income;
      rsp.connections_list.push_back(ce);
      return true;
    });

    network_zone& zone = m_network_zones.at(context.m_remote_address.get_zone());
    zone.m_peerlist.get_peerlist(rsp.local_peerlist_gray, rsp.local_peerlist_white);
    rsp.my_id = zone.m_config.m_peer_id;
    rsp.local_time = time(NULL);
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_peer_id(int command, COMMAND_REQUEST_PEER_ID::request& arg, COMMAND_REQUEST_PEER_ID::response& rsp, p2p_connection_context& context)
  {
    rsp.my_id = m_network_zones.at(context.m_remote_address.get_zone()).m_config.m_peer_id;
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_get_support_flags(int command, COMMAND_REQUEST_SUPPORT_FLAGS::request& arg, COMMAND_REQUEST_SUPPORT_FLAGS::response& rsp, p2p_connection_context& context)
  {
    rsp.support_flags = m_network_zones.at(context.m_remote_address.get_zone()).m_config.m_support_flags;
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::request_callback(const epee::net_utils::connection_context_base& context)
  {
    m_network_zones.at(context.m_remote_address.get_zone()).m_net_server.get_config_object().request_callback(context.m_connection_id);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::relay_notify_to_list(int command, const epee::span<const uint8_t> data_buff, std::vector<std::pair<epee::net_utils::zone, boost::uuids::uuid>> connections)
  {
    std::sort(connections.begin(), connections.end());
    auto zone = m_network_zones.begin();
    for(const auto& c_id: connections)
    {
      for (;;)
      {
        if (zone == m_network_zones.end())
        {
           MWARNING("Unable to relay all messages, " << epee::net_utils::zone_to_string(c_id.first) << " not available");
           return false;
        }
        if (c_id.first <= zone->first)
          break;
	  
        ++zone;
      }
      if (zone->first == c_id.first)
        zone->second.m_net_server.get_config_object().notify(command, data_buff, c_id.second);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::callback(p2p_connection_context& context)
  {
    m_payload_handler.on_callback(context);
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::invoke_notify_to_peer(int command, const epee::span<const uint8_t> req_buff, const epee::net_utils::connection_context_base& context)
  {
    if(is_filtered_command(context.m_remote_address, command))
      return false;

    network_zone& zone = m_network_zones.at(context.m_remote_address.get_zone());
    int res = zone.m_net_server.get_config_object().notify(command, req_buff, context.m_connection_id);
    return res > 0;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::invoke_command_to_peer(int command, const epee::span<const uint8_t> req_buff, std::string& resp_buff, const epee::net_utils::connection_context_base& context)
  {
    if(is_filtered_command(context.m_remote_address, command))
      return false;

    network_zone& zone = m_network_zones.at(context.m_remote_address.get_zone());
    int res = zone.m_net_server.get_config_object().invoke(command, req_buff, resp_buff, context.m_connection_id);
    return res > 0;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::drop_connection(const epee::net_utils::connection_context_base& context)
  {
    m_network_zones.at(context.m_remote_address.get_zone()).m_net_server.get_config_object().close(context.m_connection_id);
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler> template<class t_callback>
  bool node_server<t_payload_net_handler>::try_ping(basic_node_data& node_data, p2p_connection_context& context, const t_callback &cb)
  {
    if(!node_data.my_port)
      return false;

    bool address_ok = (context.m_remote_address.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id() || context.m_remote_address.get_type_id() == epee::net_utils::ipv6_network_address::get_type_id());
    CHECK_AND_ASSERT_MES(address_ok, false,
        "Only IPv4 or IPv6 addresses are supported here");

    const epee::net_utils::network_address na = context.m_remote_address;
    std::string ip;
    uint32_t ipv4_addr;
    boost::asio::ip::address_v6 ipv6_addr;
    bool is_ipv4;
    if (na.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id())
    {
      ipv4_addr = na.as<const epee::net_utils::ipv4_network_address>().ip();
      ip = epee::string_tools::get_ip_string_from_int32(ipv4_addr);
      is_ipv4 = true;
    }
    else
    {
      ipv6_addr = na.as<const epee::net_utils::ipv6_network_address>().ip();
      ip = ipv6_addr.to_string();
      is_ipv4 = false;
    }
    network_zone& zone = m_network_zones.at(na.get_zone());

    if(!zone.m_peerlist.is_host_allowed(context.m_remote_address))
      return false;

    std::string port = epee::string_tools::num_to_string_fast(node_data.my_port);

    epee::net_utils::network_address address;
    if (is_ipv4)
    {
      address = epee::net_utils::network_address{epee::net_utils::ipv4_network_address(ipv4_addr, node_data.my_port)};
    }
    else
    {
      address = epee::net_utils::network_address{epee::net_utils::ipv6_network_address(ipv6_addr, node_data.my_port)};
    }
    peerid_type pr = node_data.peer_id;
    bool r = zone.m_net_server.connect_async(ip, port, zone.m_config.m_net_config.ping_connection_timeout, [cb, /*context,*/ address, pr, this](
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

      network_zone& zone = m_network_zones.at(address.get_zone());

      bool inv_call_res = epee::net_utils::async_invoke_remote_command2<COMMAND_PING::response>(ping_context.m_connection_id, COMMAND_PING::ID, req, zone.m_net_server.get_config_object(),
        [=](int code, const COMMAND_PING::response& rsp, p2p_connection_context& context)
      {
        if(code <= 0)
        {
          LOG_WARNING_CC(ping_context, "Failed to invoke COMMAND_PING to " << address.str() << "(" << code <<  ", " << epee::levin::get_err_descr(code) << ")");
          return;
        }

        network_zone& zone = m_network_zones.at(address.get_zone());
        if(rsp.status != PING_OK_RESPONSE_STATUS_TEXT || pr != rsp.peer_id)
        {
          LOG_WARNING_CC(ping_context, "back ping invoke wrong response \"" << rsp.status << "\" from" << address.str() << ", hsh_peer_id=" << pr_ << ", rsp.peer_id=" << rsp.peer_id);
          zone.m_net_server.get_config_object().close(ping_context.m_connection_id);
          return;
        }
        zone.m_net_server.get_config_object().close(ping_context.m_connection_id);
        cb();
      });

      if(!inv_call_res)
      {
        LOG_WARNING_CC(ping_context, "back ping invoke failed to " << address.str());
        zone.m_net_server.get_config_object().close(ping_context.m_connection_id);
        return false;
      }
      return true;
    }, zone.m_bind_ip);
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
    if(context.m_remote_address.get_zone() != epee::net_utils::zone::public_)
      return false;

    COMMAND_REQUEST_SUPPORT_FLAGS::request support_flags_request;
    bool r = epee::net_utils::async_invoke_remote_command2<typename COMMAND_REQUEST_SUPPORT_FLAGS::response>
    (
      context.m_connection_id, 
      COMMAND_REQUEST_SUPPORT_FLAGS::ID, 
      support_flags_request, 
      m_network_zones.at(epee::net_utils::zone::public_).m_net_server.get_config_object(),
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
    if(!m_payload_handler.process_payload_sync_data(std::move(arg.payload_data), context, false))
    {
      LOG_WARNING_CC(context, "Failed to process_payload_sync_data(), dropping connection");
      drop_connection(context);
      return 1;
    }

    //fill response
    rsp.local_time = time(NULL);

    const epee::net_utils::zone zone_type = context.m_remote_address.get_zone();
    network_zone& zone = m_network_zones.at(zone_type);

    zone.m_peerlist.get_peerlist_head(rsp.local_peerlist_new, true);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);

    /* Tor/I2P nodes receiving connections via forwarding (from tor/i2p daemon)
    do not know the address of the connecting peer. This is relayed to them,
    iff the node has setup an inbound hidden service. The other peer will have
    to use the random peer_id value to link the two. My initial thought is that
    the inbound peer should leave the other side marked as `<unknown tor host>`,
    etc., because someone could give faulty addresses over Tor/I2P to get the
    real peer with that identity banned/blacklisted. */

    if(!context.m_is_income && zone.m_our_address.get_zone() == zone_type)
      rsp.local_peerlist_new.push_back(peerlist_entry{zone.m_our_address, zone.m_config.m_peer_id, std::time(nullptr)});

    LOG_DEBUG_CC(context, "COMMAND_TIMED_SYNC");
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  int node_server<t_payload_net_handler>::handle_handshake(int command, typename COMMAND_HANDSHAKE::request& arg, typename COMMAND_HANDSHAKE::response& rsp, p2p_connection_context& context)
  {
    if(arg.node_data.network_id != m_network_id)
    {

      LOG_INFO_CC(context, "WRONG NETWORK AGENT CONNECTED! id=" << arg.node_data.network_id);
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

    network_zone& zone = m_network_zones.at(context.m_remote_address.get_zone());

    if (zone.m_current_number_of_in_peers >= zone.m_config.m_net_config.max_in_connection_count) // in peers limit
    {
      LOG_WARNING_CC(context, "COMMAND_HANDSHAKE came, but already have max incoming connections, so dropping this one.");
      drop_connection(context);
      return 1;
    }

    if(!m_payload_handler.process_payload_sync_data(std::move(arg.payload_data), context, true))
    {
      LOG_WARNING_CC(context, "COMMAND_HANDSHAKE came, but process_payload_sync_data returned false, dropping connection.");
      drop_connection(context);
      return 1;
    }

#if !defined(LOKI_ENABLE_INTEGRATION_TEST_HOOKS)
    if(has_too_many_connections(context.m_remote_address))
    {
      LOG_PRINT_CCONTEXT_L1("CONNECTION FROM " << context.m_remote_address.host_str() << " REFUSED, too many connections from the same address");
      drop_connection(context);
      return 1;
    }
#endif

    //associate peer_id with this connection
    context.peer_id = arg.node_data.peer_id;
    context.m_in_timedsync = false;
    context.m_rpc_port = arg.node_data.rpc_port;

    if(arg.node_data.peer_id != zone.m_config.m_peer_id && arg.node_data.my_port && zone.m_can_pingback)
    {
      peerid_type peer_id_l = arg.node_data.peer_id;
      uint32_t port_l = arg.node_data.my_port;
      //try ping to be sure that we can add this peer to peer_list
      try_ping(arg.node_data, context, [peer_id_l, port_l, context, this]()
      {
        CHECK_AND_ASSERT_MES((context.m_remote_address.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id() || context.m_remote_address.get_type_id() == epee::net_utils::ipv6_network_address::get_type_id()), void(),
            "Only IPv4 or IPv6 addresses are supported here");
        //called only(!) if success pinged, update local peerlist
        peerlist_entry pe;
        const epee::net_utils::network_address na = context.m_remote_address;
        if (context.m_remote_address.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id())
        {
          pe.adr = epee::net_utils::ipv4_network_address(na.as<epee::net_utils::ipv4_network_address>().ip(), port_l);
        }
        else
        {
          pe.adr = epee::net_utils::ipv6_network_address(na.as<epee::net_utils::ipv6_network_address>().ip(), port_l);
        }
        time_t last_seen;
        time(&last_seen);
        pe.last_seen = static_cast<int64_t>(last_seen);
        pe.id = peer_id_l;
        pe.pruning_seed = context.m_pruning_seed;
        pe.rpc_port = context.m_rpc_port;
        this->m_network_zones.at(context.m_remote_address.get_zone()).m_peerlist.append_with_peer_white(pe);
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
    zone.m_peerlist.get_peerlist_head(rsp.local_peerlist_new, true);
    get_local_node_data(rsp.node_data, zone);
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
    rsp.peer_id = m_network_zones.at(context.m_remote_address.get_zone()).m_config.m_peer_id;
    return 1;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::log_peerlist()
  {
    std::vector<peerlist_entry> pl_white;
    std::vector<peerlist_entry> pl_gray;
    for (auto& zone : m_network_zones)
      zone.second.m_peerlist.get_peerlist(pl_gray, pl_white);
    MINFO(ENDL << "Peerlist white:" << ENDL << print_peerlist_to_string(pl_white) << ENDL << "Peerlist gray:" << ENDL << print_peerlist_to_string(pl_gray) );
    return true;
  }
  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::log_connections()
  {
    std::string connections = print_connections_container();
    MINFO("Connections: \r\n" << connections);
    return true;
  }
  //-----------------------------------------------------------------------------------
#if 0 // TODO: Graft: del  
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::do_supernode_announce(const cryptonote::COMMAND_RPC_SUPERNODE_ANNOUNCE::request &req)
  {
    MTRACE("Incoming supernode announce request");
#ifdef LOCK_RTA_SENDING
    return;
#endif

    MDEBUG("P2P Request: do_supernode_announce: start");

    COMMAND_SUPERNODE_ANNOUNCE::request p2p_req;
    p2p_req.supernode_public_id = req.supernode_public_id;
    p2p_req.height = req.height;
    p2p_req.signature = req.signature;
    p2p_req.network_address = req.network_address;
    p2p_req.hop = 0;

    MDEBUG("P2P Request: do_supernode_announce: announce to me");
    {
        MDEBUG("P2P Request: do_supernode_announce: lock");
        boost::unique_lock<boost::recursive_mutex> guard(m_supernode_lock);
        MDEBUG("P2P Request: do_supernode_announce: lock acquired");
        post_request_to_supernodes<cryptonote::COMMAND_RPC_SUPERNODE_ANNOUNCE>("send_supernode_announce", p2p_req);
    }

    MDEBUG("P2P Request: do_supernode_announce: prepare peerlist");
    std::string blob;
    epee::serialization::store_t_to_binary(p2p_req, blob);
    std::set<peerid_type> announced_peers;


    // first collect all connections into container
    // (its rarely possible connection could be erased from connection_map while iterating with 'foreach_connection')
    std::list<connection_info> all_connections;
    m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& context) {
      all_connections.push_back(
                    {context.m_connection_id,
                     context.peer_id,
                     epee::net_utils::print_connection_context_short(context)} );
      return true;
    });

    std::list<connection_info> random_connections;

    select_subset_with_probability(1.0 /*/ all_connections.size()*/, all_connections, random_connections);


    // same as 'relay_notify_to_list' does but we also need a) populate announced_peers and b) some extra logging
    for (const auto &c: random_connections) {
        MTRACE("[" << c.info << "] invoking COMMAND_SUPERNODE_ANNOUCE");
        if (m_net_server.get_config_object().notify(COMMAND_SUPERNODE_ANNOUNCE::ID, blob, c.id)) {
            MTRACE("[" << c.info << "] COMMAND_SUPERNODE_ANNOUCE invoked, peer_id: " << c.peer_id);
            announced_peers.insert(c.peer_id);


        }
        else
            LOG_ERROR("[" << c.info << "] failed to invoke COMMAND_SUPERNODE_ANNOUNCE");
    }
    m_announce_bytes_out += blob.size() * announced_peers.size();

    return;
    // XXX: not clear why do we need to send to "peers" if we already sent to all the connected neighbours?
    // also,

    std::list<peerlist_entry> peerlist_white, peerlist_gray;
    m_peerlist.get_peerlist_full(peerlist_gray, peerlist_white);
    std::vector<peerlist_entry> peers_to_send;
    for (auto pe :peerlist_white) {
        if (announced_peers.find(pe.id) != announced_peers.end()) {
            continue;
        }
        peers_to_send.push_back(pe);
    }
    if (peers_to_send.empty()) {
      MWARNING("P2P Request: do_supernode_announce: peers_to_send is empty");
      return;
    }
    std::vector<peerlist_entry> random_peers_to_send;
    select_subset_with_probability(1.0 / peers_to_send.size(), peers_to_send, random_peers_to_send);

    MDEBUG("P2P Request: do_supernode_announce: peers_to_send size: " << random_peers_to_send.size() << ", peerlist_white size: " << peerlist_white.size() << ", announced_peers size: " << announced_peers.size());
    MDEBUG("P2P Request: do_supernode_announce: notify_peer_list");
    notify_peer_list(COMMAND_SUPERNODE_ANNOUNCE::ID, blob, random_peers_to_send);
    MDEBUG("P2P Request: do_supernode_announce: end");
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::do_broadcast(const cryptonote::COMMAND_RPC_BROADCAST::request &req)
  {
      MDEBUG("Incoming broadcast request");

      MDEBUG("P2P Request: do_broadcast from:" << req.sender_address <<
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

      MDEBUG("P2P Request: do_broadcast: broadcast to me");
      {
          LOG_PRINT_L3("P2P Request: do_broadcast: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);
          LOG_PRINT_L3("P2P Request: do_broadcast: unlock");
          post_request_to_supernodes<cryptonote::COMMAND_RPC_BROADCAST>("broadcast", req, req.callback_uri);
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

      {
          MDEBUG("P2P Request: do_broadcast: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
          MDEBUG("P2P Request: do_broadcast: unlock");
          m_supernode_requests_cache.insert(p2p_req.message_id);
          int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
          m_supernode_requests_timestamps.insert(std::make_pair(timestamp, p2p_req.message_id));

          MDEBUG("P2P Request: do_broadcast: clean request cache");
          remove_old_request_cache();
      }

      MDEBUG("P2P Request: do_broadcast: prepare peerlist");

      std::string blob;
      epee::serialization::store_t_to_binary(p2p_req, blob);
      std::set<peerid_type> announced_peers;

      // send to peers
      std::list<connection_info> connections;
      m_net_server.get_config_object().foreach_connection([&](p2p_connection_context& context) {
          // TODO: isn't all rta peers have peer_id = 0?
          if (context.peer_id == 0) {
              LOG_INFO_CC(context, "invalid connection [COMMAND_BROADCAST]");
              return true;
          }

          connections.push_back(
          {context.m_connection_id,
           context.peer_id,
           epee::net_utils::print_connection_context_short(context)} );
          return true;
      });

      for (const auto &c: connections) {
          MTRACE("[" << c.info << "] invoking COMMAND_BROADCAST");
          if (m_net_server.get_config_object().notify(COMMAND_BROADCAST::ID, blob, c.id)) {
              MTRACE("[" << c.info << "] COMMAND_BROADCAST invoked, peer_id: " << c.peer_id);
              announced_peers.insert(c.peer_id);
          }
          else
              LOG_ERROR("[" << c.info << "] failed to invoke COMMAND_BROADCAST");
      }
      m_broadcast_bytes_out += blob.size() * announced_peers.size();

      std::list<peerlist_entry> peerlist_white, peerlist_gray;
      m_peerlist.get_peerlist_full(peerlist_gray, peerlist_white);
      std::vector<peerlist_entry> peers_to_send;
      for (auto pe :peerlist_white) {
          if (announced_peers.find(pe.id) != announced_peers.end())
              continue;
          peers_to_send.push_back(pe);
      }

      MDEBUG("P2P Request: do_broadcast: peers_to_send size: " << peers_to_send.size() << ", peerlist_white size: " << peerlist_white.size() << ", announced_peers size: " << announced_peers.size());
      MDEBUG("P2P Request: do_broadcast: notify_peer_list");
      notify_peer_list(COMMAND_BROADCAST::ID, blob, peers_to_send);
      m_broadcast_bytes_out += blob.size() * peers_to_send.size();

      MDEBUG("P2P Request: do_broadcast: End");
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::do_multicast(const cryptonote::COMMAND_RPC_MULTICAST::request &req)
  {
      MDEBUG("Incoming multicast request");

      MDEBUG("P2P Request: do_multicast: Start");

      std::string data_blob;
      epee::serialization::store_t_to_binary(req, data_blob);
      std::vector<uint8_t> data_vec(data_blob.begin(), data_blob.end());
      crypto::hash message_hash;
      if (!tools::sha256sum(data_vec.data(), data_vec.size(), message_hash))
      {
          LOG_ERROR("RTA Multicast: wrong data format for hashing!");
          return;
      }

      std::list<std::string> remaining_addresses;
      MDEBUG("P2P Request: do_multicast: multicast to me");
      {
          MDEBUG("P2P Request: do_multicast: lock");
          boost::unique_lock<boost::recursive_mutex> guard(m_supernode_lock);
          MDEBUG("P2P Request: do_multicast: unlock");
          for (auto &addr : req.receiver_addresses) {
              auto it = m_supernodes.find(addr);
              if (it != m_supernodes.end()) {
                  MDEBUG("P2P Request: do_multicast: multicast to " << addr);
                  post_request_to_supernode<cryptonote::COMMAND_RPC_MULTICAST>(it->second, "multicast", req, req.callback_uri);
              }
              else {
                  remaining_addresses.push_back(addr);
              }
          }
      }

      if (remaining_addresses.empty()) {
          LOG_PRINT_L2("P2P Request: do_multicast: End (all multicast recipients were local)");
          return;
      }

#ifdef LOCK_RTA_SENDING
    return;
#endif

      COMMAND_MULTICAST::request p2p_req = AUTO_VAL_INIT(p2p_req);
      p2p_req.receiver_addresses = remaining_addresses;
      p2p_req.sender_address = req.sender_address;
      p2p_req.callback_uri = req.callback_uri;
      p2p_req.data = req.data;
      p2p_req.wait_answer = req.wait_answer;
      p2p_req.hop = HOP_RETRIES_MULTIPLIER * get_max_hop(p2p_req.receiver_addresses);
      p2p_req.message_id = epee::string_tools::pod_to_hex(message_hash);

      {
          MDEBUG("P2P Request: do_multicast: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
          MDEBUG("P2P Request: do_multicast: unlock");
          m_supernode_requests_cache.insert(p2p_req.message_id);
          int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
          m_supernode_requests_timestamps.insert(std::make_pair(timestamp, p2p_req.message_id));

          MDEBUG("P2P Request: do_multicast: clean request cache");
          remove_old_request_cache();
      }

      MDEBUG("P2P Request: do_multicast: multicast send");
      std::string blob;
      epee::serialization::store_t_to_binary(p2p_req, blob);
      // stat counter updated in multicast_send
      multicast_send(COMMAND_MULTICAST::ID, blob, p2p_req.receiver_addresses);
      MDEBUG("P2P Request: do_multicast: End");
  }

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::do_unicast(const cryptonote::COMMAND_RPC_UNICAST::request &req)
  {
      MDEBUG("Incoming unicast request");
#ifdef LOCK_RTA_SENDING
    return;
#endif

      MDEBUG("P2P Request: do_unicast: Start sending to: " << req.receiver_address);

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

      LOG_PRINT_L2("P2P Request: do_unicast: checking unicast to me");
      {
          LOG_PRINT_L3("P2P Request: do_unicast: lock");
          boost::unique_lock<boost::recursive_mutex> guard(m_supernode_lock);
          LOG_PRINT_L3("P2P Request: do_unicast: unlock");
          const std::string &addr = req.receiver_address;
          auto it = m_supernodes.find(addr);
          if (it != m_supernodes.end()) {
              LOG_PRINT_L2("P2P Request: do_unicast: unicast to local supernode " << addr);
              post_request_to_supernode<cryptonote::COMMAND_RPC_UNICAST>(it->second, "unicast", req, req.callback_uri);
              LOG_PRINT_L2("P2P request: do_unicast: End (unicast recipient was local)");
              return;
          }
      }

      COMMAND_UNICAST::request p2p_req = AUTO_VAL_INIT(p2p_req);
      p2p_req.receiver_address = req.receiver_address;
      p2p_req.sender_address = req.sender_address;
      p2p_req.callback_uri = req.callback_uri;
      p2p_req.data = req.data;
      p2p_req.wait_answer = req.wait_answer;
      p2p_req.hop = HOP_RETRIES_MULTIPLIER * get_max_hop(addresses);
      p2p_req.message_id = epee::string_tools::pod_to_hex(message_hash);

      {
          MDEBUG("P2P Request: do_unicast: lock");
          boost::lock_guard<boost::recursive_mutex> guard(m_request_cache_lock);
          MDEBUG("P2P Request: do_unicast: unlock");
          m_supernode_requests_cache.insert(p2p_req.message_id);
          int timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
          m_supernode_requests_timestamps.insert(std::make_pair(timestamp, p2p_req.message_id));

          MDEBUG("P2P Request: do_unicast: clean request cache");
          remove_old_request_cache();
      }

      MDEBUG("P2P Request: do_unicast: unicast send");
      std::string blob;
      epee::serialization::store_t_to_binary(p2p_req, blob);
      multicast_send(COMMAND_UNICAST::ID, blob, addresses);
      MDEBUG("P2P Request: do_unicast: End");
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
          route.last_announce_height = it->second.last_announce_height;
          route.max_hop = it->second.max_hop;
          std::vector<cryptonote::peer_data> peers;
          for (auto pit = it->second.peers.begin(); pit != it->second.peers.end(); ++pit)
          {
              cryptonote::peer_data peer;
              peer.host = pit->adr.host_str();
              peer.port = pit->adr.template as<epee::net_utils::ipv4_network_address>().port();
              peer.id = pit->id;
              peer.last_seen = pit->last_seen;
              peers.push_back(peer);
          }
          route.peers = peers;
          tunnels.push_back(route);
      }
      return tunnels;
  }
#endif  

  //-----------------------------------------------------------------------------------
  template<class t_payload_net_handler>
  std::string node_server<t_payload_net_handler>::print_connections_container()
  {

    std::stringstream ss;
    for (auto& zone : m_network_zones)
    {
      zone.second.m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
      {
        ss << cntxt.m_remote_address.str()
          << " \t\tpeer_id " << cntxt.peer_id
          << " \t\tconn_id " << cntxt.m_connection_id << (cntxt.m_is_income ? " INC":" OUT")
          << std::endl;
        return true;
      });
    }
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
    network_zone& zone = m_network_zones.at(context.m_remote_address.get_zone());
    if (!zone.m_net_server.is_stop_signal_sent() && !context.m_is_income) {
      epee::net_utils::network_address na{};
      na = context.m_remote_address;

      zone.m_peerlist.remove_from_peer_anchor(na);
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
    const network_zone& public_zone = m_network_zones.at(epee::net_utils::zone::public_);
    for(const epee::net_utils::network_address& na: peers)
    {
      if(public_zone.m_net_server.is_stop_signal_sent())
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
      const uint16_t default_port = cryptonote::get_config(m_nettype).P2P_DEFAULT_PORT;
      expect<epee::net_utils::network_address> adr = net::get_network_address(pr_str, default_port);
      if (adr)
      {
        add_zone(adr->get_zone());
        container.push_back(std::move(*adr));
        continue;
      }
      std::vector<epee::net_utils::network_address> resolved_addrs;
      bool r = append_net_address(resolved_addrs, pr_str, default_port);
      CHECK_AND_ASSERT_MES(r, false, "Failed to parse or resolve address from string: " << pr_str);
      for (const epee::net_utils::network_address& addr : resolved_addrs)
      {
        container.push_back(addr);
      }
    }

    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_max_out_peers(network_zone& zone, int64_t max)
  {
    if (max == -1)
      max = P2P_DEFAULT_CONNECTIONS_COUNT_OUT;
    zone.m_config.m_net_config.max_out_connection_count = max;
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_max_in_peers(network_zone& zone, int64_t max)
  {
    if (max == -1)
      max = P2P_DEFAULT_CONNECTIONS_COUNT_IN;
    zone.m_config.m_net_config.max_in_connection_count = max;
    return true;
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::change_max_out_public_peers(size_t count)
  {
    auto public_zone = m_network_zones.find(epee::net_utils::zone::public_);
    if (public_zone != m_network_zones.end())
    {
      const auto current = public_zone->second.m_net_server.get_config_object().get_out_connections_count();
      public_zone->second.m_config.m_net_config.max_out_connection_count = count;
      if(current > count)
        public_zone->second.m_net_server.get_config_object().del_out_connections(current - count);
      m_payload_handler.set_max_out_peers(count);
    }
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::change_max_in_public_peers(size_t count)
  {
    auto public_zone = m_network_zones.find(epee::net_utils::zone::public_);
    if (public_zone != m_network_zones.end())
    {
      const auto current = public_zone->second.m_net_server.get_config_object().get_in_connections_count();
      public_zone->second.m_config.m_net_config.max_in_connection_count = count;
      if(current > count)
        public_zone->second.m_net_server.get_config_object().del_in_connections(current - count);
    }
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
    this->islimitup=(limit != -1) && (limit != default_limit_up);

    if (limit==-1) {
      limit=default_limit_up;
    }

    epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_rate_up_limit( limit );
    MINFO("Set limit-up to " << limit << " kB/s");
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_rate_down_limit(const boost::program_options::variables_map& vm, int64_t limit)
  {
    this->islimitdown=(limit != -1) && (limit != default_limit_down);
    if(limit==-1) {
      limit=default_limit_down;
    }
    epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_rate_down_limit( limit );
    MINFO("Set limit-down to " << limit << " kB/s");
    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::set_rate_limit(const boost::program_options::variables_map& vm, int64_t limit)
  {
    int64_t limit_up = 0;
    int64_t limit_down = 0;

    if(limit == -1)
    {
      limit_up = default_limit_up;
      limit_down = default_limit_down;
    }
    else
    {
      limit_up = limit;
      limit_down = limit;
    }
    if(!this->islimitup) {
      epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_rate_up_limit(limit_up);
      MINFO("Set limit-up to " << limit_up << " kB/s");
    }
    if(!this->islimitdown) {
      epee::net_utils::connection<epee::levin::async_protocol_handler<p2p_connection_context> >::set_rate_down_limit(limit_down);
      MINFO("Set limit-down to " << limit_down << " kB/s");
    }

    return true;
  }

  template<class t_payload_net_handler>
  bool node_server<t_payload_net_handler>::has_too_many_connections(const epee::net_utils::network_address &address)
  {
    if (address.get_zone() != epee::net_utils::zone::public_)
      return false; // Unable to determine how many connections from host

    const size_t max_connections = m_nettype == cryptonote::MAINNET ? 1 : 20;
    size_t count = 0;

    m_network_zones.at(epee::net_utils::zone::public_).m_net_server.get_config_object().foreach_connection([&](const p2p_connection_context& cntxt)
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
    if (m_offline) return true;
    if (!m_exclusive_peers.empty()) return true;
    if (m_payload_handler.needs_new_sync_connections()) return true;

    for (auto& zone : m_network_zones)
    {
      if (zone.second.m_net_server.is_stop_signal_sent())
        return false;

      if (zone.second.m_connect == nullptr)
        continue;

      peerlist_entry pe{};
      if (!zone.second.m_peerlist.get_random_gray_peer(pe))
        continue;

      if (!check_connection_and_handshake_with_peer(pe.adr, pe.last_seen))
      {
        zone.second.m_peerlist.remove_from_peer_gray(pe);
        LOG_PRINT_L2("PEER EVICTED FROM GRAY PEER LIST IP address: " << pe.adr.host_str() << " Peer ID: " << peerid_type(pe.id));
      }
      else
      {
        zone.second.m_peerlist.set_peer_just_seen(pe.id, pe.adr, pe.pruning_seed, pe.rpc_port);
        LOG_PRINT_L2("PEER PROMOTED TO WHITE PEER LIST IP address: " << pe.adr.host_str() << " Peer ID: " << peerid_type(pe.id));
      }
    }
    return true;
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::add_used_stripe_peer(const typename t_payload_net_handler::connection_context &context)
  {
    const uint32_t stripe = tools::get_pruning_stripe(context.m_pruning_seed);
    if (stripe == 0 || stripe > (1ul << CRYPTONOTE_PRUNING_LOG_STRIPES))
      return;
    const uint32_t index = stripe - 1;
    CRITICAL_REGION_LOCAL(m_used_stripe_peers_mutex);
    MINFO("adding stripe " << stripe << " peer: " << context.m_remote_address.str());
    std::remove_if(m_used_stripe_peers[index].begin(), m_used_stripe_peers[index].end(),
        [&context](const epee::net_utils::network_address &na){ return context.m_remote_address == na; });
    m_used_stripe_peers[index].push_back(context.m_remote_address);
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::remove_used_stripe_peer(const typename t_payload_net_handler::connection_context &context)
  {
    const uint32_t stripe = tools::get_pruning_stripe(context.m_pruning_seed);
    if (stripe == 0 || stripe > (1ul << CRYPTONOTE_PRUNING_LOG_STRIPES))
      return;
    const uint32_t index = stripe - 1;
    CRITICAL_REGION_LOCAL(m_used_stripe_peers_mutex);
    MINFO("removing stripe " << stripe << " peer: " << context.m_remote_address.str());
    std::remove_if(m_used_stripe_peers[index].begin(), m_used_stripe_peers[index].end(),
        [&context](const epee::net_utils::network_address &na){ return context.m_remote_address == na; });
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::clear_used_stripe_peers()
  {
    CRITICAL_REGION_LOCAL(m_used_stripe_peers_mutex);
    MINFO("clearing used stripe peers");
    for (auto &e: m_used_stripe_peers)
      e.clear();
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::add_upnp_port_mapping_impl(uint32_t port, bool ipv6) // if ipv6 false, do ipv4
  {
    std::string ipversion = ipv6 ? "(IPv6)" : "(IPv4)";
    MDEBUG("Attempting to add IGD port mapping " << ipversion << ".");
    int result;
    const int ipv6_arg = ipv6 ? 1 : 0;

#if MINIUPNPC_API_VERSION > 13
    // default according to miniupnpc.h
    unsigned char ttl = 2;
    UPNPDev* deviceList = upnpDiscover(1000, NULL, NULL, 0, ipv6_arg, ttl, &result);
#else
    UPNPDev* deviceList = upnpDiscover(1000, NULL, NULL, 0, ipv6_arg, &result);
#endif
    UPNPUrls urls;
    IGDdatas igdData;
    char lanAddress[64];
    result = UPNP_GetValidIGD(deviceList, &urls, &igdData, lanAddress, sizeof lanAddress);
    freeUPNPDevlist(deviceList);
    if (result > 0) {
      if (result == 1) {
        std::ostringstream portString;
        portString << port;

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

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::add_upnp_port_mapping_v4(uint32_t port)
  {
    add_upnp_port_mapping_impl(port, false);
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::add_upnp_port_mapping_v6(uint32_t port)
  {
    add_upnp_port_mapping_impl(port, true);
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::add_upnp_port_mapping(uint32_t port, bool ipv4, bool ipv6)
  {
    if (ipv4) add_upnp_port_mapping_v4(port);
    if (ipv6) add_upnp_port_mapping_v6(port);
  }


  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::delete_upnp_port_mapping_impl(uint32_t port, bool ipv6)
  {
    std::string ipversion = ipv6 ? "(IPv6)" : "(IPv4)";
    MDEBUG("Attempting to delete IGD port mapping " << ipversion << ".");
    int result;
    const int ipv6_arg = ipv6 ? 1 : 0;
#if MINIUPNPC_API_VERSION > 13
    // default according to miniupnpc.h
    unsigned char ttl = 2;
    UPNPDev* deviceList = upnpDiscover(1000, NULL, NULL, 0, ipv6_arg, ttl, &result);
#else
    UPNPDev* deviceList = upnpDiscover(1000, NULL, NULL, 0, ipv6_arg, &result);
#endif
    UPNPUrls urls;
    IGDdatas igdData;
    char lanAddress[64];
    result = UPNP_GetValidIGD(deviceList, &urls, &igdData, lanAddress, sizeof lanAddress);
    freeUPNPDevlist(deviceList);
    if (result > 0) {
      if (result == 1) {
        std::ostringstream portString;
        portString << port;

        int portMappingResult;
        portMappingResult = UPNP_DeletePortMapping(urls.controlURL, igdData.first.servicetype, portString.str().c_str(), "TCP", 0);
        if (portMappingResult != 0) {
          LOG_ERROR("UPNP_DeletePortMapping failed, error: " << strupnperror(portMappingResult));
        } else {
          MLOG_GREEN(el::Level::Info, "Deleted IGD port mapping.");
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
#if 0 // TODO: Graft: del
  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::handle_stakes_update(uint64_t block_height, const cryptonote::StakeTransactionProcessor::supernode_stake_array& stakes)
  {
    static std::string supernode_endpoint("send_supernode_stakes");

    boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);

    if (m_supernodes.empty())
      return;

    MDEBUG("handle_stakes_update to supernode for block #" << block_height);

    cryptonote::COMMAND_RPC_SUPERNODE_STAKES::request request;

    request.block_height = block_height;

    request.stakes.reserve(stakes.size());

    for (const cryptonote::supernode_stake& src_stake : stakes)
    {
      cryptonote::COMMAND_RPC_SUPERNODE_STAKES::supernode_stake dst_stake;

      dst_stake.amount = src_stake.amount;
      dst_stake.tier = src_stake.tier;
      dst_stake.block_height = src_stake.block_height;
      dst_stake.unlock_time = src_stake.unlock_time;
      dst_stake.supernode_public_id = src_stake.supernode_public_id;
      dst_stake.supernode_public_address = cryptonote::get_account_address_as_str(m_nettype, false, src_stake.supernode_public_address);

      request.stakes.emplace_back(std::move(dst_stake));
    }

    post_request_to_supernodes<cryptonote::COMMAND_RPC_SUPERNODE_STAKES>(supernode_endpoint, request);
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::send_stakes_to_supernode()
  {
    m_payload_handler.get_core().invoke_update_stakes_handler();
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::handle_blockchain_based_list_update(uint64_t block_height, const cryptonote::StakeTransactionProcessor::supernode_tier_array& tiers)
  {
    static std::string supernode_endpoint("blockchain_based_list");

    boost::lock_guard<boost::recursive_mutex> guard(m_supernode_lock);

    if (m_supernodes.empty())
      return;

    MDEBUG("handle_blockchain_based_list_update to supernode for block #" << block_height);

    cryptonote::COMMAND_RPC_SUPERNODE_BLOCKCHAIN_BASED_LIST::request request;

    request.block_height = block_height;

    for (size_t i=0; i<tiers.size(); i++)
    {
      const cryptonote::StakeTransactionProcessor::supernode_tier_array::value_type& src_tier = tiers[i];
      cryptonote::COMMAND_RPC_SUPERNODE_BLOCKCHAIN_BASED_LIST::tier                  dst_tier;

      dst_tier.supernodes.reserve(src_tier.size());

      for (const cryptonote::BlockchainBasedList::supernode& src_supernode : src_tier)
      {
        cryptonote::COMMAND_RPC_SUPERNODE_BLOCKCHAIN_BASED_LIST::supernode dst_supernode;

        dst_supernode.supernode_public_id      = src_supernode.supernode_public_id;
        dst_supernode.supernode_public_address = cryptonote::get_account_address_as_str(m_nettype, false, src_supernode.supernode_public_address);
        dst_supernode.amount                   = src_supernode.amount;

        dst_tier.supernodes.emplace_back(std::move(dst_supernode));
      }

      request.tiers.emplace_back(std::move(dst_tier));
    }

    post_request_to_supernodes<cryptonote::COMMAND_RPC_SUPERNODE_BLOCKCHAIN_BASED_LIST>(supernode_endpoint, request);
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::send_blockchain_based_list_to_supernode(uint64_t last_received_block_height)
  {
    m_payload_handler.get_core().invoke_update_blockchain_based_list_handler(last_received_block_height);
  }
#endif

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::delete_upnp_port_mapping_v4(uint32_t port)
  {
    delete_upnp_port_mapping_impl(port, false);
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::delete_upnp_port_mapping_v6(uint32_t port)
  {
    delete_upnp_port_mapping_impl(port, true);
  }

  template<class t_payload_net_handler>
  void node_server<t_payload_net_handler>::delete_upnp_port_mapping(uint32_t port)
  {
    delete_upnp_port_mapping_v4(port);
    delete_upnp_port_mapping_v6(port);
  }

  template<typename t_payload_net_handler>
  boost::optional<p2p_connection_context_t<typename t_payload_net_handler::connection_context>>
  node_server<t_payload_net_handler>::socks_connect(network_zone& zone, const epee::net_utils::network_address& remote, epee::net_utils::ssl_support_t ssl_support)
  {
    auto result = socks_connect_internal(zone.m_net_server.get_stop_signal(), zone.m_net_server.get_io_service(), zone.m_proxy_address, remote);
    if (result) // if no error
    {
      p2p_connection_context context{};
      if (zone.m_net_server.add_connection(context, std::move(*result), remote, ssl_support))
        return {std::move(context)};
    }
    return boost::none;
  }

  template<typename t_payload_net_handler>
  boost::optional<p2p_connection_context_t<typename t_payload_net_handler::connection_context>>
  node_server<t_payload_net_handler>::public_connect(network_zone& zone, epee::net_utils::network_address const& na, epee::net_utils::ssl_support_t ssl_support)
  {
    bool is_ipv4 = na.get_type_id() == epee::net_utils::ipv4_network_address::get_type_id();
    bool is_ipv6 = na.get_type_id() == epee::net_utils::ipv6_network_address::get_type_id();
    CHECK_AND_ASSERT_MES(is_ipv4 || is_ipv6, boost::none,
      "Only IPv4 or IPv6 addresses are supported here");

    std::string address;
    std::string port;

    if (is_ipv4)
    {
      const epee::net_utils::ipv4_network_address &ipv4 = na.as<const epee::net_utils::ipv4_network_address>();
      address = epee::string_tools::get_ip_string_from_int32(ipv4.ip());
      port = epee::string_tools::num_to_string_fast(ipv4.port());
    }
    else if (is_ipv6)
    {
      const epee::net_utils::ipv6_network_address &ipv6 = na.as<const epee::net_utils::ipv6_network_address>();
      address = ipv6.ip().to_string();
      port = epee::string_tools::num_to_string_fast(ipv6.port());
    }
    else
    {
      LOG_ERROR("Only IPv4 or IPv6 addresses are supported here");
      return boost::none;
    }

    typename net_server::t_connection_context con{};
    const bool res = zone.m_net_server.connect(address, port,
      zone.m_config.m_net_config.connection_timeout,
      con, zone.m_bind_ip, ssl_support);

    if (res)
      return {std::move(con)};
    return boost::none;
  }
}
