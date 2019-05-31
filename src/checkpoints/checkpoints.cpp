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

#include "checkpoints.h"

#include "common/dns_utils.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h" // epee json include
#include "serialization/keyvalue_serialization.h"
#include <vector>
#include "syncobj.h"
#include "blockchain_db/blockchain_db.h"
<<<<<<< HEAD
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "utils/utils.h"
||||||| parent of 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
=======
#include "cryptonote_basic/cryptonote_format_utils.h"
>>>>>>> 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)

using namespace epee;

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "checkpoints"

namespace cryptonote
{
 bool checkpoint_t::check(crypto::hash const &hash) const
 {
    bool result = block_hash == hash;
    if (result) MINFO   ("CHECKPOINT PASSED FOR HEIGHT " << height << " " << block_hash);
    else        MWARNING("CHECKPOINT FAILED FOR HEIGHT " << height << ". EXPECTED HASH " << block_hash << "GIVEN HASH: " << hash);
    return result;
<<<<<<< HEAD
 }
 
 height_to_hash const HARDCODED_MAINNET_CHECKPOINTS[] =
 {
   {1,     "5d03cc547e916ef79967d001288955cd5e18d3a1ae1c957c58cfd0d950fd295c"},
   {10,    "a28669967ad657355c81fa61a51d368369cf8776bdf9e9ff971bd5f922fa1303"},
   {100,   "dc6a22176a0511cc21be34eb0293cba88aacb0e29dd443bb741cc77ea00f77f9"},
   {1000,  "6f494e6c1b1fdec49a18f9c6a36c3f01f6e01cd184238c798f81f85cf04985bc"},
   {10000, "b4a749347aeb81ed13c8e78fccd7e5de01cbb9a1c7212c57a9efd056acfe7cc2"},
   {20000, "eb63c2fa28bc7442da724ce0ecbc6655a226290029251eb8be7ca0190f27ad08"},
   {30000, "4d130a125ea2483e8c1a0bc83beb24ec49426611fac3496b8954fb440d2cbbd9"},
   {50000, "2af952344a5a70ba58838ae41318a10c11b289de53d6aa33af6e3b614aa5ce08"},
   {80000, "24622bff193be66bf1bfa8938b427135e3ce8b19970281bb1e3e23f81be6c2fd"},
   {100000, "a85e2b55f2b8512d6200a63686a7ff38af4fb62c4245fcca6c19ef89bce4751a"},
   {120000, "a2ed9a924d1e41b2969a202da22b2f66df74d18c6541a9b9b686b42f166e6014"},
   {150000, "97270d631542023c0815a125834c01a8e2e419d7c8498d1e3c59a504d713f867"},
   {180000, "2d352ac387b412fc2a4bab837ec1eeb6ea30d6cd20388ae3744afd4a30c37c68"},
   {200000, "de9ab57ac93d68ca8ddfea472caeabaaf86271385b3aa1c4fa168ccbb6180a6a"},
 };
 
 crypto::hash get_newest_hardcoded_checkpoint(cryptonote::network_type nettype, uint64_t *height)
 {
   crypto::hash result = crypto::null_hash;
   *height = 0;
   if (nettype != MAINNET && nettype != TESTNET)
     return result;

   if (nettype == MAINNET)
   {
     uint64_t last_index         = Utils::array_count(HARDCODED_MAINNET_CHECKPOINTS) - 1;
     height_to_hash const &entry = HARDCODED_MAINNET_CHECKPOINTS[last_index];

     if (epee::string_tools::hex_to_pod(entry.hash, result))
       *height = entry.height;
   }
   return result;
 }

 bool load_checkpoints_from_json(const std::string &json_hashfile_fullpath, std::vector<height_to_hash> &checkpoint_hashes)
 {
   boost::system::error_code errcode;
   if (! (boost::filesystem::exists(json_hashfile_fullpath, errcode)))
   {
     LOG_PRINT_L1("Blockchain checkpoints file not found");
     return true;
   }

   height_to_hash_json hashes;
   if (!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
   {
     MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
     return false;
   }

   checkpoint_hashes = std::move(hashes.hashlines);
   return true;
 }
 
 
 //---------------------------------------------------------------------------
 bool checkpoints::get_checkpoint(uint64_t height, checkpoint_t &checkpoint) const
 {
   try
   {
     auto guard = db_rtxn_guard(m_db);
     return m_db->get_block_checkpoint(height, checkpoint);
   }
   catch (const std::exception &e)
   {
     MERROR("Get block checkpoint from DB failed at height: " << height << ", what = " << e.what());
     return false;
   }
 }
 
 //---------------------------------------------------------------------------
 bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str)
 {
     crypto::hash h = crypto::null_hash;
     bool r         = epee::string_tools::hex_to_pod(hash_str, h);
     CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");
     
     checkpoint_t checkpoint = {};
     if (get_checkpoint(height, checkpoint))
     {
         crypto::hash const &curr_hash = checkpoint.block_hash;
         CHECK_AND_ASSERT_MES(h == curr_hash, false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
     }
     else
     {
         checkpoint.type       = checkpoint_type::hardcoded;
         checkpoint.height     = height;
         checkpoint.block_hash = h;
         r                     = update_checkpoint(checkpoint);
     }
     
     return r;
 }
||||||| parent of 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
  }

  bool load_checkpoints_from_json(const std::string &json_hashfile_fullpath, std::vector<height_to_hash> &checkpoint_hashes)
  {
    boost::system::error_code errcode;
    if (! (boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    height_to_hash_json hashes;
    if (!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }

    checkpoint_hashes = std::move(hashes.hashlines);
    return true;
  }

  static bool get_checkpoint_from_db_safe(BlockchainDB const *db, uint64_t height, checkpoint_t &checkpoint)
  {
    try
    {
      return db->get_block_checkpoint(height, checkpoint);
    }
    catch (const std::exception &e)
    {
      MERROR("Get block checkpoint from DB failed at height: " << height << ", what = " << e.what());
      return false;
    }
  }

  static bool update_checkpoint_in_db_safe(BlockchainDB *db, checkpoint_t &checkpoint)
  {
    try
    {
      db->update_block_checkpoint(checkpoint);
    }
    catch (const std::exception& e)
    {
      MERROR("Failed to add checkpoint with hash: " << checkpoint.block_hash << " at height: " << checkpoint.height << ", what = " << e.what());
      return false;
    }

    return true;
  }

=======
  }

  bool load_checkpoints_from_json(const std::string &json_hashfile_fullpath, std::vector<height_to_hash> &checkpoint_hashes)
  {
    boost::system::error_code errcode;
    if (! (boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    height_to_hash_json hashes;
    if (!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }

    checkpoint_hashes = std::move(hashes.hashlines);
    return true;
  }

  static bool get_checkpoint_from_db_safe(BlockchainDB *db, uint64_t height, checkpoint_t &checkpoint)
  {
    try
    {
      auto guard = db_rtxn_guard(db);
      return db->get_block_checkpoint(height, checkpoint);
    }
    catch (const std::exception &e)
    {
      MERROR("Get block checkpoint from DB failed at height: " << height << ", what = " << e.what());
      return false;
    }
  }

  static bool update_checkpoint_in_db_safe(BlockchainDB *db, checkpoint_t const &checkpoint)
  {
    try
    {
      auto guard = db_wtxn_guard(db);
      db->update_block_checkpoint(checkpoint);
    }
    catch (const std::exception& e)
    {
      MERROR("Failed to add checkpoint with hash: " << checkpoint.block_hash << " at height: " << checkpoint.height << ", what = " << e.what());
      return false;
    }

    return true;
  }
>>>>>>> 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
  //---------------------------------------------------------------------------
  void checkpoints::block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs)
  {
    uint64_t const height = get_block_height(block);
    if (height < service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL ||
        block.major_version < network_version_12_checkpointing)
      return;

    uint64_t const end_cull_height = height - service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL;
    uint64_t start_cull_height     = (end_cull_height < service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL)
                                     ? 0
                                     : end_cull_height - service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL;
    start_cull_height += (start_cull_height % service_nodes::CHECKPOINT_INTERVAL);
    m_last_cull_height = std::max(m_last_cull_height, start_cull_height);

    auto guard = db_wtxn_guard(m_db);
    for (; m_last_cull_height < end_cull_height; m_last_cull_height += service_nodes::CHECKPOINT_INTERVAL)
    {
<<<<<<< HEAD
      if (m_last_cull_height % service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL == 0)
        continue;

      try
      {
        m_db->remove_block_checkpoint(m_last_cull_height);
      }
      catch (const std::exception &e)
      {
        MERROR("Pruning block checkpoint on block added failed non-trivially at height: " << m_last_cull_height << ", what = " << e.what());
      }
    }
  }
  //---------------------------------------------------------------------------
  void checkpoints::blockchain_detached(uint64_t height)
  {
    m_last_cull_height = std::min(m_last_cull_height, height);

    checkpoint_t top_checkpoint;
    auto guard = db_wtxn_guard(m_db);
    if (m_db->get_top_checkpoint(top_checkpoint))
||||||| parent of 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
      crypto::hash const &curr_hash = checkpoint.block_hash;
      CHECK_AND_ASSERT_MES(h == curr_hash, false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    else
    {
      checkpoint.height       = height;
      checkpoint.type         = checkpoint_type::hardcoded;
      checkpoint.block_hash   = h;
      r = update_checkpoint_in_db_safe(m_db, checkpoint);
    }

    return r;
  }
  //---------------------------------------------------------------------------
  static bool add_vote_if_unique(checkpoint_t &checkpoint, service_nodes::checkpoint_vote const &vote)
  {
    CHECK_AND_ASSERT_MES(checkpoint.block_hash == vote.block_hash,
                         false,
                         "Developer error: Add vote if unique should only be called when the vote hash and checkpoint hash match");

    // TODO(doyle): Factor this out, a similar function exists in service node deregister
    CHECK_AND_ASSERT_MES(vote.voters_quorum_index < service_nodes::QUORUM_SIZE,
                         false,
                         "Vote is indexing out of bounds");

    const auto signature_it = std::find_if(checkpoint.signatures.begin(), checkpoint.signatures.end(), [&vote](service_nodes::voter_to_signature const &check) {
      return vote.voters_quorum_index == check.quorum_index;
    });

    if (signature_it == checkpoint.signatures.end())
    {
      service_nodes::voter_to_signature new_voter_to_signature = {};
      new_voter_to_signature.quorum_index                      = vote.voters_quorum_index;
      new_voter_to_signature.signature                         = vote.signature;
      checkpoint.signatures.push_back(new_voter_to_signature);
      return true;
    }

    return false;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint_vote(service_nodes::checkpoint_vote const &vote)
  {
   // TODO(doyle): Always accept votes. Later on, we should only accept votes up
   // to a certain point, so that eventually once a certain number of blocks
   // have transpired, we can go through all our "seen" votes and deregister
   // anyone who has not participated in voting by that point.

#if 0
    uint64_t newest_checkpoint_height = get_max_height();
    if (vote.block_height < newest_checkpoint_height)
      return true;
#endif

    // TODO(doyle): Double work. Factor into a generic vote checker as we already have one in service node deregister
    std::array<int, service_nodes::QUORUM_SIZE> unique_vote_set = {};
    std::vector<checkpoint_t> &candidate_checkpoints            = m_staging_points[vote.block_height];
    std::vector<checkpoint_t>::iterator curr_checkpoint         = candidate_checkpoints.end();
    for (auto it = candidate_checkpoints.begin(); it != candidate_checkpoints.end(); it++)
=======
      crypto::hash const &curr_hash = checkpoint.block_hash;
      CHECK_AND_ASSERT_MES(h == curr_hash, false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    else
    {
      checkpoint.type       = checkpoint_type::hardcoded;
      checkpoint.height     = height;
      checkpoint.block_hash = h;
      r                     = update_checkpoint(checkpoint);
    }

    return r;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::update_checkpoint(checkpoint_t const &checkpoint)
  {
    // TODO(doyle): Verify signatures and hash check out
    std::array<size_t, service_nodes::CHECKPOINT_QUORUM_SIZE> unique_vote_set = {};
    if (checkpoint.type == checkpoint_type::service_node)
>>>>>>> 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
    {
<<<<<<< HEAD
      uint64_t start_height = top_checkpoint.height;
      for (size_t delete_height = start_height;
           delete_height > height;
           delete_height -= service_nodes::CHECKPOINT_INTERVAL)
||||||| parent of 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
      checkpoint_t const &checkpoint = *it;
      if (checkpoint.block_hash == vote.block_hash)
        curr_checkpoint = it;

      for (service_nodes::voter_to_signature const &vote_to_sig : checkpoint.signatures)
=======
      CHECK_AND_ASSERT_MES(checkpoint.signatures.size() >= service_nodes::CHECKPOINT_MIN_VOTES, false, "Checkpoint has insufficient signatures to be considered");
      for (service_nodes::voter_to_signature const &vote_to_sig : checkpoint.signatures)
>>>>>>> 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
      {
<<<<<<< HEAD
        if (delete_height % service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL == 0)
          continue;

        try
        {
          m_db->remove_block_checkpoint(delete_height);
        }
        catch (const std::exception &e)
        {
          MERROR("Remove block checkpoint on detach failed non-trivially at height: " << delete_height << ", what = " << e.what());
        }
||||||| parent of 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
        if (vote_to_sig.quorum_index > unique_vote_set.size())
          return false;

        if (++unique_vote_set[vote_to_sig.quorum_index] > 1)
        {
          // NOTE: Voter is trying to vote twice
          return false;
        }
=======
        ++unique_vote_set[vote_to_sig.voter_index];
        CHECK_AND_ASSERT_MES(vote_to_sig.voter_index < service_nodes::CHECKPOINT_QUORUM_SIZE, false, "Vote is indexing out of bounds");
        CHECK_AND_ASSERT_MES(unique_vote_set[vote_to_sig.voter_index] == 1, false, "Voter is trying to vote twice");
>>>>>>> 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
      }
    }
<<<<<<< HEAD
||||||| parent of 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)

    if (curr_checkpoint == candidate_checkpoints.end())
    {
      checkpoint_t new_checkpoint = {};
      new_checkpoint.height       = vote.block_height;
      new_checkpoint.type         = checkpoint_type::service_node;
      new_checkpoint.block_hash   = vote.block_hash;
      candidate_checkpoints.push_back(new_checkpoint);
      curr_checkpoint = (candidate_checkpoints.end() - 1);
    }

    if (add_vote_if_unique(*curr_checkpoint, vote))
    {
      if (curr_checkpoint->signatures.size() > service_nodes::MIN_VOTES_TO_CHECKPOINT)
      {
        update_checkpoint_in_db_safe(m_db, *curr_checkpoint);
      }
    }

    return true;
=======
    else
    {
      CHECK_AND_ASSERT_MES(checkpoint.signatures.size() == 0, false, "Non service-node checkpoints should have no signatures");
    }

    bool result = update_checkpoint_in_db_safe(m_db, checkpoint);
    return result;
>>>>>>> 8af377d2b... Unify and move responsibility of voting to quorum_cop (#615)
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_in_checkpoint_zone(uint64_t height) const
  {
    uint64_t top_checkpoint_height = 0;
    checkpoint_t top_checkpoint;
    if (m_db->get_top_checkpoint(top_checkpoint))
      top_checkpoint_height = top_checkpoint.height;
    
    return height <= top_checkpoint_height;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool* is_a_checkpoint, bool * rta_checkpoint) const
  {
    checkpoint_t checkpoint;
    bool found = get_checkpoint(height, checkpoint);
    if (is_a_checkpoint) *is_a_checkpoint = found;
    if (rta_checkpoint) *rta_checkpoint = false;

    if(!found)
      return true;

    bool result = checkpoint.check(h);
    if (rta_checkpoint)
      *rta_checkpoint = (checkpoint.type == checkpoint_type::supernode);

    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height, bool *rta_checkpoint)
  {
     if (rta_checkpoint)
       *rta_checkpoint = false;
 
     if (0 == block_height)
       return false;
 
     {   
       std::vector<checkpoint_t> const first_checkpoint = m_db->get_checkpoints_range(0, blockchain_height, 1); 
       if (first_checkpoint.empty() || blockchain_height < first_checkpoint[0].height)
         return true;
     }   
 
     checkpoint_t immutable_checkpoint;
     uint64_t immutable_height = 0;
     if (m_db->get_immutable_checkpoint(&immutable_checkpoint, blockchain_height))
     {   
       immutable_height = immutable_checkpoint.height;
       if (rta_checkpoint)
         *rta_checkpoint = (immutable_checkpoint.type == checkpoint_type::supernode);
     }   
 
     m_immutable_height = std::max(immutable_height, m_immutable_height);
     bool result        = block_height > m_immutable_height;
     return result;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_max_height() const
  {
    uint64_t result = 0;
    checkpoint_t top_checkpoint;
    if (m_db->get_top_checkpoint(top_checkpoint))
      result = top_checkpoint.height;

    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::init(network_type nettype, struct BlockchainDB *db)
  {
    *this = {};
    m_db = db;
    *this     = {};
    m_db      = db;
    m_nettype = nettype;

    if (db->is_read_only())
      return true;

    if (nettype == MAINNET)
    {
      for (size_t i = 0; i < Utils::array_count(HARDCODED_MAINNET_CHECKPOINTS); ++i)
      {
        height_to_hash const &checkpoint = HARDCODED_MAINNET_CHECKPOINTS[i];
        ADD_CHECKPOINT(checkpoint.height, checkpoint.hash);
      }
    }
    return true;
    
  }

}
