// Copyright (c)      2018, The Graft Project
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

#include "checkpoint_vote_handler.h"
#include "rta_voting.h"
#include "cryptonote_config.h"
#include "graft_rta_config.h"
#include "cryptonote_core.h"
#include "blockchain_based_list.h"

#include "version.h"
#include "common/util.h"
#include "net/local_ip.h"
#include <boost/endian/conversion.hpp>


#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "CheckpointVoteHandler"

namespace rta
{


static bool handle_checkpoint_vote(cryptonote::core& core, const checkpoint_vote& vote, const std::vector<pool_vote_entry>& votes, const quorum& quorum);


CheckpointVoteHandler::CheckpointVoteHandler(cryptonote::core& core)
  : m_core(core), m_last_checkpointed_height(0)
{
}

CheckpointVoteHandler::~CheckpointVoteHandler()
{
  
}

void CheckpointVoteHandler::init()
{
  m_last_checkpointed_height = 0;
}




void CheckpointVoteHandler::blockchain_detached(uint64_t height, bool by_pop_blocks)
{
  uint8_t hf_version                        = m_core.get_hard_fork_version(height);
  uint64_t const REORG_SAFETY_BUFFER_BLOCKS = (hf_version >= cryptonote::network_version_18_checkpointing)
      ? config::graft::REORG_SAFETY_BUFFER_BLOCKS_POST_HF18
      : config::graft::REORG_SAFETY_BUFFER_BLOCKS_PRE_HF18;
  
  if (m_last_checkpointed_height >= height + REORG_SAFETY_BUFFER_BLOCKS)
  {
    if (!by_pop_blocks)
    {
      LOG_ERROR("The blockchain was detached to height: " << height << ", but quorum cop has already processed votes for checkpointing up to " << m_last_checkpointed_height);
      LOG_ERROR("This implies a reorg occured that was over " << REORG_SAFETY_BUFFER_BLOCKS << ". This should rarely happen! Please report this to the devs.");
    }
    m_last_checkpointed_height = height - (height % config::graft::CHECKPOINT_INTERVAL);
  }
  
  m_vote_pool.remove_expired_votes(height);
}


bool CheckpointVoteHandler::handle_vote(const checkpoint_vote &vote, cryptonote::vote_verification_context &vvc)
{
  vvc = {};
  
  if (!verify_vote_age(vote, m_core.get_current_blockchain_height(), vvc))
    return false;

  // get checkpointing sample;
  cryptonote::BlockchainBasedList::supernode_array checkpoint_sample;
  if (!m_core.get_stake_tx_processor().build_checkpointing_sample(vote.block_hash, vote.block_height, checkpoint_sample)) {
    MERROR("Failed to build checkpointing sample for height: " << vote.block_height);
    vvc.m_invalid_block_height = true;
    return false;
  }
  
  rta::quorum quorum;
  quorum.from_supernode_list(checkpoint_sample);
  
  if (!verify_vote_signature(m_core.get_hard_fork_version(vote.block_height), vote, vvc, quorum))
    return false;

  std::vector<pool_vote_entry> votes = m_vote_pool.add_pool_vote_if_unique(vote, vvc);
  if (!vvc.m_added_to_pool) // NOTE: Not unique vote
    return true;

  return handle_checkpoint_vote(m_core, vote, votes, quorum);  
}



void CheckpointVoteHandler::set_votes_relayed(std::vector<checkpoint_vote> const &relayed_votes)
{
  m_vote_pool.set_relayed(relayed_votes);
}

std::vector<checkpoint_vote> CheckpointVoteHandler::get_relayable_votes(uint64_t current_height, uint8_t hf_version)
{
  return m_vote_pool.get_relayable_votes(current_height, hf_version, false);
}

void CheckpointVoteHandler::process_quorums(const cryptonote::block &/*block*/)
{
  // Graft: checkpoint voting done on supernode
}

int find_index_in_quorum_group(std::vector<crypto::public_key> const &group, crypto::public_key const &my_pubkey)
{
  int result = -1;
  auto it = std::find(group.begin(), group.end(), my_pubkey);
  if (it == group.end()) return result;
  result = std::distance(group.begin(), it);
  return result;
}

bool CheckpointVoteHandler::block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs, cryptonote::checkpoint_t const * /*checkpoint*/)
{
  process_quorums(block);
  uint64_t const height = cryptonote::get_block_height(block) + 1; // chain height = new top block height + 1
  m_vote_pool.remove_expired_votes(height);
  m_vote_pool.remove_used_votes(txs, block.major_version);
  
 
  return true;
}


static bool handle_checkpoint_vote(cryptonote::core& core, const checkpoint_vote& vote, const std::vector<pool_vote_entry>& votes, const quorum& quorum)
{
  if (votes.size() < config::graft::CHECKPOINT_MIN_VOTES)
  {
    LOG_PRINT_L2("Don't have enough votes yet to submit a checkpoint: have " << votes.size() << " of " << config::graft::CHECKPOINT_MIN_VOTES << " required");
    return true;
  }
  MDEBUG(__FUNCTION__);
  cryptonote::checkpoint_t checkpoint{};
  cryptonote::Blockchain &blockchain = core.get_blockchain_storage();
  
  // NOTE: Multiple network threads are going to try and update the
  // checkpoint, blockchain.update_checkpoint does NOT do any
  // validation- that is done here since we want to keep code for
  // converting votes to data suitable for the DB in service node land.
  
  // So then, multiple threads can race to update the checkpoint. One
  // thread could retrieve an outdated checkpoint whilst another has
  // already updated it. i.e. we could replace a checkpoint with lesser
  // votes prematurely. The actual update in the DB is an atomic
  // operation, but this check and validation step is NOT, taking the
  // lock here makes it so.
  
  std::unique_lock<cryptonote::Blockchain> lock{blockchain};
  
  bool update_checkpoint;
  if (blockchain.get_checkpoint(vote.block_height, checkpoint) &&
      checkpoint.block_hash == vote.block_hash)
  {
    MDEBUG(__FUNCTION__ << " existing checkpoint.");
    update_checkpoint = false;
    if (checkpoint.signatures.size() != config::graft::CHECKPOINT_QUORUM_SIZE)
    {
      checkpoint.signatures.reserve(config::graft::CHECKPOINT_QUORUM_SIZE);
      std::sort(checkpoint.signatures.begin(),
                checkpoint.signatures.end(),
                [](const auto &lhs, const auto &rhs) {
        return lhs.key_index < rhs.key_index;
      });
      
      for (const pool_vote_entry &pool_vote : votes)
      {
        auto it = std::lower_bound(checkpoint.signatures.begin(),
                                   checkpoint.signatures.end(),
                                   pool_vote,
                                   [](const auto &lhs, const auto &vote) {
            return lhs.key_index < vote.vote.signature.key_index;
        });
        
        if (it == checkpoint.signatures.end() ||
            pool_vote.vote.signature.key_index != it->key_index)
        {
          update_checkpoint = true;
          checkpoint.signatures.insert(it, pool_vote.vote.signature);
        }
      }
    }
  }
  else
  {
    MDEBUG(__FUNCTION__ << " new checkpoint.");
    update_checkpoint = true;
    checkpoint = make_empty_rta_checkpoint(vote.block_hash, vote.block_height);
    
    checkpoint.signatures.reserve(votes.size());
    for (pool_vote_entry const &pool_vote : votes)
      checkpoint.signatures.push_back(pool_vote.vote.signature);
  }
  
  if (update_checkpoint)
    blockchain.update_checkpoint(checkpoint);
  
  return true;
}


uint64_t quorum_checksum(const std::vector<crypto::public_key> &pubkeys, size_t offset) {
  constexpr size_t KEY_BYTES = sizeof(crypto::public_key);
  
  // Calculate a checksum by reading bytes 0-7 from the first pubkey as a little-endian uint64_t,
  // then reading 1-8 from the second pubkey, 2-9 from the third, and so on, and adding all the
  // uint64_t values together.  If we get to 25 we wrap the read around the end and keep going.
  uint64_t sum = 0;
  alignas(uint64_t) std::array<char, sizeof(uint64_t)> local;
  for (auto &pk : pubkeys) {
    offset %= KEY_BYTES;
    auto *pkdata = reinterpret_cast<const char *>(&pk);
    if (offset <= KEY_BYTES - sizeof(uint64_t))
      std::memcpy(local.data(), pkdata + offset, sizeof(uint64_t));
    else {
      size_t prewrap = KEY_BYTES - offset;
      std::memcpy(local.data(), pkdata + offset, prewrap);
      std::memcpy(local.data() + prewrap, pkdata, sizeof(uint64_t) - prewrap);
    }
    sum += boost::endian::little_to_native(*reinterpret_cast<uint64_t *>(local.data()));
    ++offset;
  }
  return sum;
}

}
