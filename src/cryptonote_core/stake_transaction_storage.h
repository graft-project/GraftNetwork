#pragma once

#include <cryptonote_config.h>
#include <list>

#include "crypto/hash.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "serialization/serialization.h"

namespace cryptonote
{

struct stake_transaction
{
  crypto::hash hash;
  unsigned int tier; //based from index 0
  uint64_t block_height;
  uint64_t unlock_time;
  std::string supernode_public_id;
  cryptonote::account_public_address supernode_public_address;
  crypto::signature supernode_signature;
  crypto::secret_key tx_secret_key;

  BEGIN_SERIALIZE_OBJECT()
    FIELD(hash)
    FIELD(tier)
    FIELD(block_height)
    FIELD(unlock_time)
    FIELD(supernode_public_id)
    FIELD(supernode_public_address)
    FIELD(supernode_signature)
    FIELD(tx_secret_key)
  END_SERIALIZE()
};

class StakeTransactionTiers
{
public:
    //  50,000 GRFT –  tier 1
    //  90,000 GRFT –  tier 2
    //  150,000 GRFT – tier 3
    //  250,000 GRFT – tier 4
    static constexpr uint64_t TIER1_STAKE_AMOUNT = COIN *  50000;
    static constexpr uint64_t TIER2_STAKE_AMOUNT = COIN *  90000;
    static constexpr uint64_t TIER3_STAKE_AMOUNT = COIN * 150000;
    static constexpr uint64_t TIER4_STAKE_AMOUNT = COIN * 250000;
};

class StakeTransactionStorage
{
public:
  typedef std::vector<stake_transaction> stake_transaction_array;
  typedef std::list<crypto::hash>        block_hash_list;

  StakeTransactionStorage(const std::string& storage_file_name);

  /// Get number of transactions
  size_t get_tx_count() const { return m_stake_txs.size(); }

  /// Get transactions
  const stake_transaction_array& get_txs() const { return m_stake_txs; }

  /// Index of last processed block
  uint64_t get_last_processed_block_index() const { return m_last_processed_block_index; }
  
  /// Get array of last block hashes
  const crypto::hash& get_last_processed_block_hash() const;

  /// Add new processed block
  void add_last_processed_block(uint64_t index, const crypto::hash& hash);

  /// Remove processed block
  void remove_last_processed_block();

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
  block_hash_list m_last_processed_block_hashes;
  size_t m_last_processed_block_hashes_count;
  stake_transaction_array m_stake_txs;
};

}
