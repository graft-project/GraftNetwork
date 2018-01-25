#pragma once

#include "crypto/hash.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "serialization/serialization.h"

namespace cryptonote
{

struct stake_transaction
{
  crypto::hash hash;
  uint64_t block_height;
  uint64_t unlock_time;
  std::string supernode_public_id;
  cryptonote::account_public_address supernode_public_address;
  crypto::signature supernode_signature;
  crypto::secret_key tx_secret_key;

  BEGIN_SERIALIZE_OBJECT()
    FIELD(hash)
    FIELD(block_height)
    FIELD(unlock_time)
    FIELD(supernode_public_id)
    FIELD(supernode_public_address)
    FIELD(supernode_signature)
    FIELD(tx_secret_key)
  END_SERIALIZE()
};

class StakeTransactionStorage
{
public:
  typedef std::vector<stake_transaction> stake_transaction_array;

  StakeTransactionStorage(const std::string& storage_file_name);

  /// Get number of transactions
  size_t get_tx_count() const { return m_stake_txs.size(); }

  /// Get transactions
  const stake_transaction_array& get_txs() const { return m_stake_txs; }

  /// Index of last processed block
  uint64_t get_last_processed_block_index() const { return m_last_processed_block_index; }
  
  /// Set index of last processed block
  void set_last_processed_block_index(uint64_t index) { m_last_processed_block_index = index; }

  /// Add transaction
  void add_tx(const stake_transaction&);

  /// Save storage to file
  void store() const;

private:
  /// Load storage from file
  void load();

private:
  std::string m_storage_file_name;
  uint64_t m_last_processed_block_index;
  stake_transaction_array m_stake_txs;
};

}
