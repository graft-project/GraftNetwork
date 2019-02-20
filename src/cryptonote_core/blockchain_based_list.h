#pragma once

#include <random>

#include "blockchain.h"
#include "cryptonote_core/stake_transaction_storage.h"

namespace cryptonote
{

class BlockchainBasedList
{
public:
  struct supernode
  {    
    std::string supernode_public_id;
    uint64_t block_height;
    uint64_t unlock_time;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(block_height)
      FIELD(unlock_time)
      FIELD(supernode_public_id)
    END_SERIALIZE()
  };

  typedef std::vector<supernode>           supernode_array;
  typedef std::vector<supernode_array>     supernode_tier_array;
  typedef std::list<supernode_tier_array>  list_history;

  /// Constructors
  BlockchainBasedList(const std::string& file_name);

  /// List of tiers
  const supernode_tier_array& tiers() const;

  /// Height of the corresponding block
  uint64_t block_height() const { return m_block_height; }

  /// Apply new block on top of the list
  void apply_block(uint64_t block_height, const crypto::hash& block_hash, StakeTransactionStorage& stake_txs);

  /// Remove latest block
  void remove_latest_block();

  /// Save list to file
  void store() const;

  /// Is the list requires store
  bool need_store() const { return m_need_store; }

private:
  /// Load list from file
  void load();

  /// Select supernodes from a list
  void select_supernodes(size_t max_items_count, const supernode_array& src_list, supernode_array& dst_list);

private:
  std::string m_storage_file_name;
  list_history m_history;
  uint64_t m_block_height;
  size_t m_history_depth;
  std::mt19937_64 m_rng;
  mutable bool m_need_store;
};

}
