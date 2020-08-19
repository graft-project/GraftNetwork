// Copyright (c) 2018, The Loki Project
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

#include <vector>
#include <unordered_map>
#include <utility>

#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/blobdatatype.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_core/checkpoint_vote_handler.h"

#include "string_tools.h"
#include "math_helper.h"
#include "syncobj.h"

#include <boost/serialization/base_object.hpp>

namespace cryptonote
{
struct tx_verification_context;
struct vote_verification_context;
struct checkpoint_t;
};

namespace rta
{


struct quorum
{
  std::vector<crypto::public_key> validators; // Array of public keys identifying service nodes who validate and sign.
  
  BEGIN_SERIALIZE()
    FIELD(validators)
  END_SERIALIZE()
};

//enum struct quorum_type : uint8_t
//{
//  checkpointing,
//  _count
//};

//inline std::ostream &operator<<(std::ostream &os, quorum_type v) {
//  switch(v)
//  {
//  case quorum_type::checkpointing: return os << "checkpointing";
//  default: assert(false);          return os << "xx_unhandled_type";
//  }
//}


//  void vote_to_blob(const quorum_vote_t& vote, unsigned char blob[]);
//  void blob_to_vote(const unsigned char blob[], quorum_vote_t& vote);
// struct service_node_keys;

cryptonote::checkpoint_t make_empty_rta_checkpoint(crypto::hash const &block_hash, uint64_t height);

bool verify_checkpoint (uint8_t hf_version, cryptonote::checkpoint_t const &checkpoint, rta::quorum const &quorum);
// TODO: quorum_vote_t (all possible types of the votes) vs checkpoint_vote ?
bool verify_vote_age (const checkpoint_vote & vote, uint64_t latest_height, cryptonote::vote_verification_context &vvc);
// TODO: 
bool verify_vote_signature (uint8_t hf_version, const checkpoint_vote& vote, cryptonote::vote_verification_context &vvc, const rta::quorum &quorum);


struct pool_vote_entry
{
  checkpoint_vote vote;
  uint64_t      time_last_sent_p2p;
};

struct voting_pool
{
  // return: The vector of votes if the vote is valid (and even if it is not unique) otherwise nullptr
  std::vector<pool_vote_entry> add_pool_vote_if_unique(const checkpoint_vote &vote, cryptonote::vote_verification_context &vvc);
  
  // TODO(loki): Review relay behaviour and all the cases when it should be triggered
  void                         set_relayed         (const std::vector<checkpoint_vote>& votes);
  void                         remove_expired_votes(uint64_t height);
  void                         remove_used_votes   (std::vector<cryptonote::transaction> const &txs, uint8_t hard_fork_version);
  
  /// Returns relayable votes for either p2p (quorum_relay=false) or quorumnet
  /// (quorum_relay=true).  Before HF14 everything goes via p2p; starting in HF14 obligation votes
  /// go via quorumnet, checkpoints go via p2p.
  std::vector<checkpoint_vote>   get_relayable_votes (uint64_t height, uint8_t hf_version, bool quorum_relay) const;
  bool                         received_checkpoint_vote(uint64_t height, size_t index_in_quorum) const;
  
private:
  std::vector<pool_vote_entry> *find_vote_pool(const checkpoint_vote &vote, bool create_if_not_found = false);
  
  struct checkpoint_pool_entry
  {
    explicit checkpoint_pool_entry(const checkpoint_vote &vote) : height{vote.block_height}, hash{vote.block_hash} {}
    checkpoint_pool_entry(uint64_t height, crypto::hash const &hash): height(height), hash(hash) {}
    uint64_t                     height;
    crypto::hash                 hash;
    std::vector<pool_vote_entry> votes;
    
    bool operator==(const checkpoint_pool_entry &e) const { return height == e.height && hash == e.hash; }
  };
  std::vector<checkpoint_pool_entry> m_checkpoint_pool;
  
  mutable epee::critical_section m_lock;
};
}; // namespace service_nodes

