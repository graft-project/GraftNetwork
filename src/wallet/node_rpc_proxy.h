// Copyright (c) 2017-2019, The Monero Project
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

#pragma once

#include <string>
#include <mutex>
#include "include_base_utils.h"
#include "net/http_client.h"
#include "rpc/core_rpc_server_commands_defs.h"

namespace tools
{

class NodeRPCProxy
{
public:
  NodeRPCProxy(epee::net_utils::http::http_simple_client &http_client, std::recursive_mutex &mutex);

  void invalidate();
  void set_offline(bool offline) { m_offline = offline; }

  boost::optional<std::string> get_rpc_version(uint32_t &version) const;
  boost::optional<std::string> get_height(uint64_t &height) const;
  void set_height(uint64_t h);
  boost::optional<std::string> get_target_height(uint64_t &height) const;
  boost::optional<std::string> get_immutable_height(uint64_t &height) const;
  boost::optional<std::string> get_block_weight_limit(uint64_t &block_weight_limit) const;
  boost::optional<std::string> get_earliest_height(uint8_t version, uint64_t &earliest_height) const;
  boost::optional<std::string> get_dynamic_base_fee_estimate(uint64_t grace_blocks, cryptonote::byte_and_output_fees &fees) const;
  boost::optional<std::string> get_fee_quantization_mask(uint64_t &fee_quantization_mask) const;
  boost::optional<uint8_t>     get_hardfork_version() const;

  std::vector<cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response::entry>             get_service_nodes(std::vector<std::string> const &pubkeys, boost::optional<std::string> &failed) const;
  std::vector<cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response::entry>             get_all_service_nodes(boost::optional<std::string> &failed) const;
  std::vector<cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response::entry>             get_contributed_service_nodes(const std::string &contributor, boost::optional<std::string> &failed) const;
  std::vector<cryptonote::COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES::entry> get_service_node_blacklisted_key_images(boost::optional<std::string> &failed) const;
  std::vector<cryptonote::COMMAND_RPC_LNS_OWNERS_TO_NAMES::response_entry>            lns_owners_to_names(cryptonote::COMMAND_RPC_LNS_OWNERS_TO_NAMES::request const &request, boost::optional<std::string> &failed) const;
  std::vector<cryptonote::COMMAND_RPC_LNS_NAMES_TO_OWNERS::response_entry>            lns_names_to_owners(cryptonote::COMMAND_RPC_LNS_NAMES_TO_OWNERS::request const &request, boost::optional<std::string> &failed) const;

private:
  boost::optional<std::string> get_info() const;

  epee::net_utils::http::http_simple_client &m_http_client;
  std::recursive_mutex &m_daemon_rpc_mutex;
  bool m_offline;

  mutable uint64_t m_service_node_blacklisted_key_images_cached_height;
  mutable std::vector<cryptonote::COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES::entry> m_service_node_blacklisted_key_images;

  bool update_all_service_nodes_cache(uint64_t height, boost::optional<std::string> &failed) const;

  mutable uint64_t m_all_service_nodes_cached_height;
  mutable std::vector<cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response::entry> m_all_service_nodes;

  mutable uint64_t m_contributed_service_nodes_cached_height;
  mutable std::string m_contributed_service_nodes_cached_address;
  mutable std::vector<cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response::entry> m_contributed_service_nodes;

  mutable uint64_t m_height;
  mutable uint64_t m_immutable_height;
  mutable uint64_t m_earliest_height[256];
  mutable cryptonote::byte_and_output_fees m_dynamic_base_fee_estimate;
  mutable uint64_t m_dynamic_base_fee_estimate_cached_height;
  mutable uint64_t m_dynamic_base_fee_estimate_grace_blocks;
  mutable uint64_t m_fee_quantization_mask;
  boost::optional<std::string> refresh_dynamic_base_fee_cache(uint64_t grace_blocks) const;
  mutable uint32_t m_rpc_version;
  mutable uint64_t m_target_height;
  mutable uint64_t m_block_weight_limit;
  mutable time_t m_get_info_time;
};

}
