#pragma once

#include <random>

#include "blockchain.h"
#include "serialization/crypto.h"
#include "serialization/list.h"
#include "serialization/vector.h"
#include "serialization/string.h"
#include "cryptonote_core/stake_transaction_storage.h"

namespace cryptonote
{

class BlockchainBasedList
{
public:
  struct supernode
  {    
    // TODO: use crypto::public_key instead 
    std::string supernode_public_id;
    cryptonote::account_public_address supernode_public_address;
    uint64_t amount;
    uint64_t block_height;
    uint64_t unlock_time;
    
    bool verify_signature(const crypto::hash &hash, const crypto::signature &signature);

    BEGIN_SERIALIZE_OBJECT()
      FIELD(amount)
      FIELD(block_height)
      FIELD(unlock_time)
      FIELD(supernode_public_id)
      FIELD(supernode_public_address)
    END_SERIALIZE()
  };

  typedef std::vector<supernode>           supernode_array;
  typedef std::vector<supernode_array>     supernode_tier_array;
  typedef std::list<supernode_tier_array>  list_history;

  /// Constructors
  BlockchainBasedList(const std::string& file_name, uint64_t first_block_number);

  /// List of tiers
  const supernode_tier_array& tiers(size_t depth = 0) const;

  /// Height of the corresponding block
  uint64_t block_height() const { return m_block_height; }

  /// Number of blocks in history
  uint64_t history_depth() const { return m_history_depth; }

  /// Apply new block on top of the list
  void apply_block(uint64_t block_height, const crypto::hash& block_hash, StakeTransactionStorage& stake_txs);

  /// Remove latest block
  void remove_latest_block();

  /// Save list to file
  void store() const;

  /// Is the list requires store
  bool need_store() const { return m_need_store; }
  
  /// Builds checkpointing sample
  static bool build_checkpointing_sample(StakeTransactionStorage& stake_txs_storage, const crypto::hash &block_hash, uint64_t height, supernode_array &result);
  
  /// Randomly selects items from source list to dest list; Caller is responsible of initalizing rng
  template<typename Vector>
  static void select_random_items(std::mt19937_64 &rng, size_t max_items_count, const Vector &src, Vector &dst)
  {
    size_t src_list_size = src.size();
  
    if (max_items_count > src_list_size)
      max_items_count = src_list_size;
  
    for (size_t i=0; i < src_list_size; i++)
    {
      size_t random_value = rng() % (src_list_size - i);
  
      if (random_value >= max_items_count)
        continue;
  
      dst.push_back(src[i]);
  
      max_items_count--;
    }
  }

private:
  /// Load list from file
  void load();

  /// Select supernodes from a list
  static void select_supernodes(std::mt19937_64 &rng, size_t max_items_count, const supernode_array& src_list, supernode_array& dst_list);

private:
  std::string m_storage_file_name;
  list_history m_history;
  uint64_t m_block_height;
  size_t m_history_depth;
  std::mt19937_64 m_rng;
  uint64_t m_first_block_number;
  mutable bool m_need_store;
};

}
