#pragma once

#include <unordered_set>

#include "blockchain.h"
#include "cryptonote_core/stake_transaction_storage.h"

namespace cryptonote
{

class BlockchainBasedList
{
public:
  struct SupernodeDesc
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

  typedef std::vector<SupernodeDesc> SupernodeList;

  /// Constructors
  BlockchainBasedList(const std::string& file_name);

  /// List of supernodes
  const SupernodeList& supernodes() const { return m_supernodes; }

  /// Height of the corresponding block
  uint64_t block_height() const { return m_block_height; }

  /// Apply new block on top of the list
  void apply_block(uint64_t block_height, const crypto::hash& block_hash, StakeTransactionStorage& stake_txs);

  /// Save list to file
  void store() const;

  /// Is the list requires store
  bool need_store() const { return m_need_store; }

private:
  /// Load list from file
  void load();

  /// Select supernodes from a list
  void select_supernodes(const crypto::hash& block_hash, size_t max_items_count, const SupernodeList& src_list, SupernodeList& dst_list);

private:
  std::string m_storage_file_name;
  SupernodeList m_supernodes;
  uint64_t m_block_height;
  std::unordered_set<std::string> selected_supernodes_cache;
  mutable bool m_need_store;
};

}
