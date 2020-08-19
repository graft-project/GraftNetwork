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

#include "rta_voting.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/verification_context.h"
#include "cryptonote_basic/connection_context.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "checkpoints/checkpoints.h"
#include "common/util.h"
#include "graft_rta_config.h"

#include "misc_log_ex.h"
#include "string_tools.h"

#include <random>
#include <string>
#include <vector>

#include <boost/endian/conversion.hpp>

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "rta_voting"

namespace rta
{


  static bool bounds_check_validator_index(rta::quorum const &quorum, uint32_t validator_index, cryptonote::vote_verification_context *vvc)
  {
    if (validator_index >= quorum.validators.size())
    {
      if (vvc) vvc->m_validator_index_out_of_bounds = true;
      LOG_PRINT_L1("Validator's index was out of bounds: " << validator_index << ", expected to be in range of: [0, " << quorum.validators.size() << ")");
      return false;
    }
    return true;
  }

  bool verify_checkpoint(uint8_t hf_version, cryptonote::checkpoint_t const &checkpoint, rta::quorum const &quorum)
  {
    if (checkpoint.type == cryptonote::checkpoint_type::supernode)
    {
      if ((checkpoint.height % config::graft::CHECKPOINT_INTERVAL) != 0)
      {
        LOG_PRINT_L1("Checkpoint given but not expecting a checkpoint at height: " << checkpoint.height);
        return false;
      }

      if (checkpoint.signatures.size() < config::graft::CHECKPOINT_MIN_VOTES)
      {
        LOG_PRINT_L1("Checkpoint has insufficient signatures to be considered at height: " << checkpoint.height);
        return false;
      }

      if (checkpoint.signatures.size() > config::graft::CHECKPOINT_QUORUM_SIZE)
      {
        LOG_PRINT_L1("Checkpoint has too many signatures to be considered at height: " << checkpoint.height);
        return false;
      }

      std::array<size_t, config::graft::CHECKPOINT_QUORUM_SIZE> unique_vote_set = {};
      for (size_t i = 0; i < checkpoint.signatures.size(); i++)
      {
        const cryptonote::rta_signature  &rta_signature = checkpoint.signatures[i];
        if (hf_version >= cryptonote::network_version_13_enforce_checkpoints && i < (checkpoint.signatures.size() - 1))
        {
          auto curr = checkpoint.signatures[i].voter_index;
          auto next = checkpoint.signatures[i + 1].voter_index;

          if (curr >= next)
          {
            LOG_PRINT_L1("Voters in checkpoints are not given in ascending order, checkpoint failed verification at height: " << checkpoint.height);
            return false;
          }
        }

        if (!bounds_check_validator_index(quorum, voter_to_signature.voter_index, nullptr)) return false;
        crypto::public_key const &key = quorum.validators[voter_to_signature.voter_index];
        if (unique_vote_set[voter_to_signature.voter_index]++)
        {
          LOG_PRINT_L1("Voter: " << epee::string_tools::pod_to_hex(key) << ", quorum index is duplicated: " << voter_to_signature.voter_index << ", checkpoint failed verification at height: " << checkpoint.height);
          return false;
        }

        if (!crypto::check_signature(checkpoint.block_hash, key, voter_to_signature.signature))
        {
          LOG_PRINT_L1("Invalid signatures for votes, checkpoint failed verification at height: " << checkpoint.height << " for voter: " << epee::string_tools::pod_to_hex(key));
          return false;
        }
      }
    }
    else
    {
      if (checkpoint.signatures.size() != 0)
      {
        LOG_PRINT_L1("Non service-node checkpoints should have no signatures, checkpoint failed at height: " << checkpoint.height);
        return false;
      }
    }

    return true;
  }

  cryptonote::checkpoint_t make_empty_service_node_checkpoint(crypto::hash const &block_hash, uint64_t height)
  {
    cryptonote::checkpoint_t result = {};
    result.type                     = cryptonote::checkpoint_type::service_node;
    result.height                   = height;
    result.block_hash               = block_hash;
    return result;
  }

  bool verify_vote_age(const checkpoint_vote& vote, uint64_t latest_height, cryptonote::vote_verification_context &vvc)
  {
    bool result           = true;
    bool height_in_buffer = false;
    if (latest_height > vote.block_height + VOTE_LIFETIME)
    {
      height_in_buffer = latest_height <= vote.block_height + (VOTE_LIFETIME + VOTE_OR_TX_VERIFY_HEIGHT_BUFFER);
      LOG_PRINT_L1("Received vote for height: " << vote.block_height << ", is older than: " << VOTE_LIFETIME
                                                << " blocks and has been rejected.");
      vvc.m_invalid_block_height = true;
    }
    else if (vote.block_height > latest_height)
    {
      height_in_buffer = vote.block_height <= latest_height + VOTE_OR_TX_VERIFY_HEIGHT_BUFFER;
      LOG_PRINT_L1("Received vote for height: " << vote.block_height << ", is newer than: " << latest_height
                                                << " (latest block height) and has been rejected.");
      vvc.m_invalid_block_height = true;
    }

    if (vvc.m_invalid_block_height)
    {
      vvc.m_verification_failed = !height_in_buffer;
      result = false;
    }

    return result;
  }

  bool verify_vote_signature(uint8_t hf_version, const checkpoint_vote &vote, cryptonote::vote_verification_context &vvc, const service_nodes::quorum &quorum)
  {
    bool result = true;
    if (vote.type > tools::enum_top<quorum_type>)
    {
      vvc.m_invalid_vote_type = true;
      result = false;
    }

    if (vote.group > quorum_group::worker || vote.group < quorum_group::validator)
    {
      vvc.m_incorrect_voting_group = true;
      result = false;
    }

    if (!result)
      return result;

    if (vote.group == quorum_group::validator)
      result = bounds_check_validator_index(quorum, vote.index_in_group, &vvc);
    else
      result = bounds_check_worker_index(quorum, vote.index_in_group, &vvc);

    if (!result)
      return result;

    crypto::public_key key = crypto::null_pkey;
    crypto::hash hash      = crypto::null_hash;

    switch(vote.type)
    {
      default:
      {
        LOG_PRINT_L1("Unhandled vote type with value: " << (int)vote.type);
        assert("Unhandled vote type" == 0);
        return false;
      };

      case quorum_type::obligations:
      {
        if (vote.group != quorum_group::validator)
        {
          LOG_PRINT_L1("Vote received specifies incorrect voting group, expected vote from validator");
          vvc.m_incorrect_voting_group = true;
          result = false;
        }
        else
        {
          key = quorum.validators[vote.index_in_group];
          hash = make_state_change_vote_hash(vote.block_height, vote.state_change.worker_index, vote.state_change.state);
          result = bounds_check_worker_index(quorum, vote.state_change.worker_index, &vvc);
        }
      }
      break;

      case quorum_type::checkpointing:
      {
        if (vote.group != quorum_group::validator)
        {
          LOG_PRINT_L1("Vote received specifies incorrect voting group");
          vvc.m_incorrect_voting_group = true;
          result = false;
        }
        else
        {
          key  = quorum.validators[vote.index_in_group];
          hash = vote.checkpoint.block_hash;
        }
      }
      break;
    }

    if (!result)
      return result;

    result = crypto::check_signature(hash, key, vote.signature);
    if (result)
      MDEBUG("Signature accepted for " << vote.type << " voter " << vote.index_in_group << "/" << key
              << (vote.type == quorum_type::obligations ? " voting for worker " + std::to_string(vote.state_change.worker_index) : "")
              << " at height " << vote.block_height);
    else
      vvc.m_signature_not_valid = true;

    return result;
  }

  template <typename T>
  static std::vector<pool_vote_entry> *find_vote_in_pool(std::vector<T> &pool, const checkpoint_vote &vote, bool create) {
    T typed_vote{vote};
    auto it = std::find(pool.begin(), pool.end(), typed_vote);
    if (it != pool.end())
      return &it->votes;
    if (!create)
      return nullptr;
    pool.push_back(std::move(typed_vote));
    return &pool.back().votes;
  }

  std::vector<pool_vote_entry> *voting_pool::find_vote_pool(const checkpoint_vote &find_vote, bool create_if_not_found) {
    switch(find_vote.type)
    {
      default:
        LOG_PRINT_L1("Unhandled find_vote type with value: " << (int)find_vote.type);
        assert("Unhandled find_vote type" == 0);
        return nullptr;

      case quorum_type::obligations:
        return find_vote_in_pool(m_obligations_pool, find_vote, create_if_not_found);

      case quorum_type::checkpointing:
        return find_vote_in_pool(m_checkpoint_pool, find_vote, create_if_not_found);
    }
  }

  void voting_pool::set_relayed(const std::vector<checkpoint_vote>& votes)
  {
    CRITICAL_REGION_LOCAL(m_lock);
    const time_t now = time(NULL);

    for (const checkpoint_vote &find_vote : votes)
    {
      std::vector<pool_vote_entry> *vote_pool = find_vote_pool(find_vote);

      if (vote_pool) // We found the group that this vote belongs to
      {
        auto vote = std::find_if(vote_pool->begin(), vote_pool->end(), [&find_vote](pool_vote_entry const &entry) {
            return (find_vote.index_in_group == entry.vote.index_in_group);
        });

        if (vote != vote_pool->end())
        {
          vote->time_last_sent_p2p = now;
        }
      }
    }
  }

  template <typename T>
  static void append_relayable_votes(std::vector<checkpoint_vote> &result, const T &pool, const uint64_t max_last_sent, uint64_t min_height) {
    for (const auto &pool_entry : pool)
      for (const auto &vote_entry : pool_entry.votes)
        if (vote_entry.vote.block_height >= min_height && vote_entry.time_last_sent_p2p <= max_last_sent)
          result.push_back(vote_entry.vote);
  }

  std::vector<checkpoint_vote> voting_pool::get_relayable_votes(uint64_t height, uint8_t hf_version, bool quorum_relay) const
  {
    CRITICAL_REGION_LOCAL(m_lock);

    // TODO(doyle): Rate-limiting: A better threshold value that follows suite with transaction relay time back-off
#if defined(LOKI_ENABLE_INTEGRATION_TEST_HOOKS)
    constexpr uint64_t TIME_BETWEEN_RELAY = 0;
#else
    constexpr uint64_t TIME_BETWEEN_RELAY = 60 * 2;
#endif

    const uint64_t max_last_sent = static_cast<uint64_t>(time(nullptr)) - TIME_BETWEEN_RELAY;
    const uint64_t min_height = height > VOTE_LIFETIME ? height - VOTE_LIFETIME : 0;

    std::vector<checkpoint_vote> result;

    if (quorum_relay && hf_version < cryptonote::network_version_14_blink)
      return result; // no quorum relaying before HF14

    if (hf_version < cryptonote::network_version_14_blink || quorum_relay)
      append_relayable_votes(result, m_obligations_pool, max_last_sent, min_height);

    if (hf_version < cryptonote::network_version_14_blink || !quorum_relay)
      append_relayable_votes(result, m_checkpoint_pool,  max_last_sent, min_height);

    return result;
  }

  // return: True if the vote was unique
  static bool add_vote_to_pool_if_unique(std::vector<pool_vote_entry> &votes, checkpoint_vote const &vote)
  {
    auto vote_it = std::lower_bound(
        votes.begin(), votes.end(), vote, [](pool_vote_entry const &pool_entry, checkpoint_vote const &vote) {
          assert(pool_entry.vote.group == vote.group);
          return pool_entry.vote.index_in_group < vote.index_in_group;
        });

    if (vote_it == votes.end() || vote_it->vote.index_in_group != vote.index_in_group)
    {
      votes.insert(vote_it, {vote});
      return true;
    }

    return false;
  }

  std::vector<pool_vote_entry> voting_pool::add_pool_vote_if_unique(const checkpoint_vote& vote, cryptonote::vote_verification_context& vvc)
  {
    CRITICAL_REGION_LOCAL(m_lock);
    auto *votes = find_vote_pool(vote, /*create_if_not_found=*/ true);
    if (!votes)
      return {};

    vvc.m_added_to_pool = add_vote_to_pool_if_unique(*votes, vote);
    return *votes;
  }

  void voting_pool::remove_used_votes(std::vector<cryptonote::transaction> const &txs, uint8_t hard_fork_version)
  {
    // TODO(doyle): Cull checkpoint votes
    CRITICAL_REGION_LOCAL(m_lock);
    if (m_obligations_pool.empty())
      return;

    for (const auto &tx : txs)
    {
      if (tx.type != cryptonote::txtype::state_change)
        continue;

      cryptonote::tx_extra_service_node_state_change state_change;
      if (!get_service_node_state_change_from_tx_extra(tx.extra, state_change, hard_fork_version))
      {
        LOG_ERROR("Could not get state change from tx, possibly corrupt tx");
        continue;
      }

      auto it = std::find(m_obligations_pool.begin(), m_obligations_pool.end(), state_change);

      if (it != m_obligations_pool.end())
        m_obligations_pool.erase(it);
    }
  }

  template <typename T>
  static void cull_votes(std::vector<T> &vote_pool, uint64_t min_height, uint64_t max_height)
  {
    for (auto it = vote_pool.begin(); it != vote_pool.end(); )
    {
      const T &pool_entry = *it;
      if (pool_entry.height < min_height || pool_entry.height > max_height)
        it = vote_pool.erase(it);
      else
        ++it;
    }
  }

  void voting_pool::remove_expired_votes(uint64_t height)
  {
    CRITICAL_REGION_LOCAL(m_lock);
    uint64_t min_height = (height < VOTE_LIFETIME) ? 0 : height - VOTE_LIFETIME;
    cull_votes(m_obligations_pool, min_height, height);
    cull_votes(m_checkpoint_pool, min_height, height);
  }

  bool voting_pool::received_checkpoint_vote(uint64_t height, size_t index_in_quorum) const
  {
    auto pool_it = std::find_if(m_checkpoint_pool.begin(),
                                m_checkpoint_pool.end(),
                                [height](checkpoint_pool_entry const &entry) { return entry.height == height; });

    if (pool_it == m_checkpoint_pool.end())
      return false;

    for (auto it = pool_it->votes.begin(); it != pool_it->votes.end(); it++)
    {
      if (it->vote.index_in_group == index_in_quorum)
        return true;
    }

    return false;
  }

  void vote_to_blob(const checkpoint_vote& vote, unsigned char blob[])
  {
    blob[0] = vote.version;
    blob[1] = static_cast<uint8_t>(vote.type);
    for (size_t i = 2; i < 8; i++)
      blob[i] = 0; // padding
    {
      uint64_t height = boost::endian::native_to_little(vote.block_height);
      std::memcpy(&blob[8], &height, 8);
    }
    blob[16] = static_cast<uint8_t>(vote.group);
    blob[17] = 0; // padding
    {
      uint16_t iig = boost::endian::native_to_little(vote.index_in_group);
      std::memcpy(&blob[18], &iig, 2);
    }
    std::memcpy(&blob[20], &vote.signature, 64);
    for (size_t i = 84; i < 88; i++)
      blob[i] = 0; // padding
    if (vote.type == quorum_type::checkpointing)
    {
      std::memcpy(&blob[84], &vote.checkpoint, 32);
      for (size_t i = 116; i < 120; i++)
        blob[i] = 0; // padding
    }
    else
    {
      uint16_t wi = boost::endian::native_to_little(vote.state_change.worker_index);
      uint16_t st = boost::endian::native_to_little(static_cast<uint16_t>(vote.state_change.state));
      std::memcpy(&blob[84], &wi, 2);
      std::memcpy(&blob[86], &st, 2);
      for (size_t i = 88; i < 120; i++)
        blob[i] = 0;
    }
  }

  void blob_to_vote(const unsigned char blob[], checkpoint_vote& vote)
  {
    vote.version = blob[0];
    vote.type = static_cast<quorum_type>(blob[1]);
    std::memcpy(&vote.block_height, &blob[8], 8); boost::endian::little_to_native_inplace(vote.block_height);
    vote.group = static_cast<quorum_group>(blob[16]);
    std::memcpy(&vote.index_in_group, &blob[18], 2); boost::endian::little_to_native_inplace(vote.index_in_group);
    std::memcpy(&vote.signature, &blob[20], 64);
    if (vote.type == quorum_type::checkpointing)
    {
      std::memcpy(&vote.checkpoint, &blob[84], 32);
    }
    else
    {
      std::memcpy(&vote.state_change.worker_index, &blob[84], 2); boost::endian::little_to_native_inplace(vote.state_change.worker_index);
      uint16_t st;
      std::memcpy(&st, &blob[86], 2); vote.state_change.state = static_cast<new_state>(boost::endian::little_to_native(st));
    }
  }
}; // namespace service_nodes

