#pragma once

#include <cryptonote_config.h>
#include <list>
#include <unordered_map>

#include "crypto/hash.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "serialization/serialization.h"

namespace cryptonote
{

struct stake_transaction
{
  crypto::hash hash;
  uint64_t amount;
  uint64_t block_height;
  uint64_t unlock_time;
  std::string supernode_public_id;
  cryptonote::account_public_address supernode_public_address;
  crypto::signature supernode_signature;
  crypto::secret_key tx_secret_key;

  bool is_valid(uint64_t block_index) const;

  BEGIN_SERIALIZE_OBJECT()
    FIELD(amount)
    FIELD(hash)
    FIELD(block_height)
    FIELD(unlock_time)
    FIELD(supernode_public_id)
    FIELD(supernode_public_address)
    FIELD(supernode_signature)
    FIELD(tx_secret_key)
  END_SERIALIZE()
};

struct supernode_stake
{
  uint64_t amount;
  unsigned int tier; //based from index 0
  uint64_t block_height;
  uint64_t unlock_time;
  std::string supernode_public_id;
  cryptonote::account_public_address supernode_public_address;
};

class StakeTransactionStorage
{
public:
  typedef std::vector<stake_transaction> stake_transaction_array;
  typedef std::list<crypto::hash>        block_hash_list;
  typedef std::vector<supernode_stake>   supernode_stake_array;

  StakeTransactionStorage(const std::string& storage_file_name, uint64_t first_block_number);

  /// Get number of transactions
  size_t get_tx_count() const { return m_stake_txs.size(); }

  /// Get transactions
  const stake_transaction_array& get_txs() const { return m_stake_txs; }

  /// Index of last processed block
  uint64_t get_last_processed_block_index() const { return m_last_processed_block_index; }
  
  /// Get array of last block hashes
  const crypto::hash& get_last_processed_block_hash() const;

  /// Has last processed blocks
  bool has_last_processed_block() const { return m_last_processed_block_hashes_count > 0; }

  /// Add new processed block
  void add_last_processed_block(uint64_t index, const crypto::hash& hash);

  /// Remove processed block
  void remove_last_processed_block();

  /// Add transaction
  void add_tx(const stake_transaction&);

  /// List of supernode stakes
  const supernode_stake_array& get_supernode_stakes(uint64_t block_number);

  /// Search supernode stake by supernode public id (returns nullptr if no stake is found)
  const supernode_stake* find_supernode_stake(uint64_t block_number, const std::string& supernode_public_id);

  /// Update supernode stakes
  void update_supernode_stakes(uint64_t block_number);

  /// Clear supernode stakes
  void clear_supernode_stakes();

  /// Save storage to file
  void store() const;

  /// Is the list requires store
  bool need_store() const { return m_need_store; }

private:
  /// Load storage from file
  void load();

  typedef std::unordered_map<std::string, size_t> supernode_stake_index_map;

private:
  std::string m_storage_file_name;
  uint64_t m_last_processed_block_index;
  block_hash_list m_last_processed_block_hashes;
  size_t m_last_processed_block_hashes_count;
  stake_transaction_array m_stake_txs;
  uint64_t m_supernode_stakes_update_block_number;
  supernode_stake_array m_supernode_stakes;
  supernode_stake_index_map m_supernode_stake_indexes;
  uint64_t m_first_block_number;
  mutable bool m_need_store;
};

}
