#pragma once

#include <functional>
#include <memory>

#include "blockchain.h"
#include "cryptonote_core/blockchain_based_list.h"
#include "cryptonote_core/stake_transaction_storage.h"

namespace cryptonote
{

class StakeTransactionProcessor
{
public:
  typedef StakeTransactionStorage::supernode_stake_array supernode_stake_array;
  typedef StakeTransactionStorage::supernode_disqualification_array supernode_disqualification_array;

  StakeTransactionProcessor(Blockchain& blockchain);

  /// Initialize storages
  void init_storages(const std::string& config_dir);

  /// Search supernode stake by supernode public id (returns nullptr if no stake is found)
  const supernode_stake* find_supernode_stake(uint64_t block_number, const std::string& supernode_public_id) const;

  /// Synchronize with blockchain
  void synchronize();

  typedef std::function<void(uint64_t block_number, const supernode_stake_array&, const supernode_disqualification_array&)> supernode_stakes_update_handler;

  /// Update handler for new stakes
  void set_on_update_stakes_handler(const supernode_stakes_update_handler&);

  /// Force invoke update handler for stakes
  void invoke_update_stakes_handler(bool force = true);

  typedef BlockchainBasedList::supernode_tier_array supernode_tier_array;
  typedef std::function<void(uint64_t block_number, const crypto::hash& block_hash, const supernode_tier_array&)> blockchain_based_list_update_handler;

  /// Update handler for new blockchain based list
  void set_on_update_blockchain_based_list_handler(const blockchain_based_list_update_handler&);

  /// Force invoke update handler for blockchain based list
  void invoke_update_blockchain_based_list_handler(bool force = true, size_t depth = 1);

  /// Check that transaction with tx_hash does not exist yet. Those to be disqualified, and signers are in corresponding sets.
  bool check_disqualification_transaction(const transaction& tx, const crypto::hash tx_hash);
  bool check_disqualification2_transaction(const transaction& tx, const crypto::hash tx_hash);

private:
  void init_storages_impl();
  void process_block(uint64_t block_index, const block& block, const crypto::hash& block_hash, bool update_storage = true);
  void invoke_update_stakes_handler_impl(uint64_t block_index);
  void invoke_update_blockchain_based_list_handler_impl(size_t depth);
  void process_block_stake_transaction(uint64_t block_index, const block& block, const crypto::hash& block_hash, bool update_storage = true);
  void process_block_blockchain_based_list(uint64_t block_index, const block& block, const crypto::hash& block_hash, bool update_storage = true);

  void process_disqualification_transaction(const transaction& tx, const crypto::hash tx_hash, uint64_t block_index, const crypto::hash& block_hash, StakeTransactionStorage::disqualification_array& disquals);
  void process_disqualification2_transaction(const transaction& tx, const crypto::hash tx_hash, uint64_t block_index, const crypto::hash& block_hash, StakeTransactionStorage::disqualification2_storage_array& disquals2);

private:
  std::string m_config_dir;
  Blockchain& m_blockchain;
  std::unique_ptr<StakeTransactionStorage> m_storage;
  std::unique_ptr<BlockchainBasedList> m_blockchain_based_list;
  mutable epee::critical_section m_storage_lock;
  supernode_stakes_update_handler m_on_stakes_update;
  blockchain_based_list_update_handler m_on_blockchain_based_list_update;
  bool m_stakes_need_update;
  bool m_blockchain_based_list_need_update;
};

}
