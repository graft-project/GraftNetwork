// Copyright (c)      2020, The Graft Project
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

#pragma once

#include "serialization/serialization.h"
#include "cryptonote_protocol/cryptonote_protocol_handler_common.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_core/rta_voting.h"
#include "cryptonote_core/checkpoint_vote.h"
#include "cryptonote_core/blockchain.h"

namespace cryptonote
{
  class core;
  struct vote_verification_context;
  struct checkpoint_t;
};

namespace rta
{
 


class CheckpointVoteHandler
    : public cryptonote::BlockAddedHook,
    public cryptonote::BlockchainDetachedHook,
    public cryptonote::InitHook
{
public:
  explicit CheckpointVoteHandler(cryptonote::core& core);
  virtual ~CheckpointVoteHandler();
  
  void init() override;
  bool block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs, const cryptonote::checkpoint_t  * /*checkpoint*/) override;
  void blockchain_detached(uint64_t height, bool by_pop_blocks) override;
  
  bool  handle_vote(const checkpoint_vote &vote, cryptonote::vote_verification_context &vvc);
  
  void  set_votes_relayed  (const std::vector<checkpoint_vote>  &relayed_votes);
  std::vector<checkpoint_vote> get_relayable_votes(uint64_t current_height, uint8_t hf_version);
  
  
private:
  
  void process_quorums(cryptonote::block const &block);
  
  cryptonote::core& m_core;
  voting_pool       m_vote_pool;
  uint64_t          m_last_checkpointed_height;
  mutable epee::critical_section m_lock;
};
 
}
