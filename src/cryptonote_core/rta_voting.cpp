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


  static bool bounds_check_voter_index(rta::quorum const &quorum, uint32_t voter_index, cryptonote::vote_verification_context *vvc)
  {
    if (voter_index >= quorum.voters.size())
    {
      if (vvc) vvc->m_voter_index_out_of_bounds = true;
      LOG_PRINT_L1("Validator's index was out of bounds: " 
                   << voter_index 
                   << ", expected to be in range of: [0, " << quorum.voters.size() << ")");
      return false;
    }
    return true;
  }

  bool verify_checkpoint(uint8_t hf_version, cryptonote::checkpoint_t const &checkpoint, const rta::quorum  &quorum)
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
        
        if (hf_version >= cryptonote::network_version_19_enforce_checkpoints && i < (checkpoint.signatures.size() - 1))
        {
          auto curr = checkpoint.signatures[i].key_index;
          auto next = checkpoint.signatures[i + 1].key_index;

          if (curr >= next)
          {
            LOG_PRINT_L1("Voters in checkpoints are not given in ascending order, checkpoint failed verification at height: " << checkpoint.height);
            return false;
          }
        }

        if (!bounds_check_voter_index(quorum, checkpoint.signatures[i].key_index, nullptr)) 
          return false;
        crypto::public_key const &key = quorum.voters[checkpoint.signatures[i].key_index];
        if (unique_vote_set[checkpoint.signatures[i].key_index]++)
        {
          LOG_PRINT_L1("Voter: " << epee::string_tools::pod_to_hex(key) << ", quorum index is duplicated: " << checkpoint.signatures[i].key_index << ", checkpoint failed verification at height: " << checkpoint.height);
          return false;
        }

        if (!crypto::check_signature(checkpoint.block_hash, key, checkpoint.signatures[i].signature))
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
        LOG_PRINT_L1("Non supernode checkpoints should have no signatures, checkpoint failed at height: " << checkpoint.height);
        return false;
      }
    }

    return true;
  }


  bool verify_vote_age(const checkpoint_vote& vote, uint64_t latest_height, cryptonote::vote_verification_context &vvc)
  {
    bool result           = true;
    bool height_in_buffer = false;
    if (latest_height > vote.block_height + config::graft::VOTE_LIFETIME)
    {
      height_in_buffer = latest_height <= vote.block_height + (config::graft::VOTE_LIFETIME + config::graft::VOTE_OR_TX_VERIFY_HEIGHT_BUFFER);
      LOG_PRINT_L1("Received vote for height: " << vote.block_height << ", is older than: " << config::graft::VOTE_LIFETIME
                                                << " blocks and has been rejected.");
      vvc.m_invalid_block_height = true;
    }
    else if (vote.block_height > latest_height)
    {
      height_in_buffer = vote.block_height <= latest_height + config::graft::VOTE_OR_TX_VERIFY_HEIGHT_BUFFER;
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

  bool verify_vote_signature(uint8_t hf_version, const checkpoint_vote &vote, cryptonote::vote_verification_context &vvc, const rta::quorum &quorum)
  {
    bool result = true;
    
    result = bounds_check_voter_index(quorum, vote.signature.key_index, &vvc);

    if (!result)
      return result;

    crypto::public_key key = crypto::null_pkey;
    crypto::hash hash      = crypto::null_hash;

    key  = quorum.voters[vote.signature.key_index];
    hash = vote.block_hash;
  

    result = crypto::check_signature(hash, key, vote.signature.signature);
    if (result)
      MDEBUG("Signature accepted for voter: "  << vote.signature.key_index << "/" << key
              << " at height " << vote.block_height);
    else
      vvc.m_signature_not_valid = true;

    return result;
  }

  template <typename T>
  static std::vector<pool_vote_entry> *find_vote_in_pool(std::vector<T> &pool, const checkpoint_vote &vote, bool create) 
  {
    T typed_vote{vote};
    auto it = std::find(pool.begin(), pool.end(), typed_vote);
    if (it != pool.end())
      return &it->votes;
    if (!create)
      return nullptr;
    pool.push_back(std::move(typed_vote));
    return &pool.back().votes;
  }

  std::vector<pool_vote_entry> *voting_pool::find_vote_pool(const checkpoint_vote &find_vote, bool create_if_not_found) 
  {
    return find_vote_in_pool(m_checkpoint_pool, find_vote, create_if_not_found);
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
            return find_vote.signature.key_index == entry.vote.signature.key_index;
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

    constexpr uint64_t TIME_BETWEEN_RELAY = 60 * 2;

    const uint64_t max_last_sent = static_cast<uint64_t>(time(nullptr)) - TIME_BETWEEN_RELAY;
    const uint64_t min_height = height > config::graft::VOTE_LIFETIME ? height - config::graft::VOTE_LIFETIME : 0;

    std::vector<checkpoint_vote> result;

    
    append_relayable_votes(result, m_checkpoint_pool,  max_last_sent, min_height);

    return result;
  }

  // return: True if the vote was unique
  static bool add_vote_to_pool_if_unique(std::vector<pool_vote_entry> &votes, checkpoint_vote const &vote)
  {
    auto vote_it = std::lower_bound(
        votes.begin(), votes.end(), vote, [](pool_vote_entry const &pool_entry, checkpoint_vote const &vote) {
          return pool_entry.vote.signature.key_index < vote.signature.key_index;
        });

    if (vote_it == votes.end() || vote_it->vote.signature.key_index != vote.signature.key_index)
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
    uint64_t min_height = (height < config::graft::VOTE_LIFETIME) ? 0 : height - config::graft::VOTE_LIFETIME;
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
      if (it->vote.signature.key_index == index_in_quorum)
        return true;
    }

    return false;
  }

  
}; // namespace service_nodes

