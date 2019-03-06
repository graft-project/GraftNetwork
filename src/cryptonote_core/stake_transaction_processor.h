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
  typedef StakeTransactionStorage::stake_transaction_array stake_transaction_array;

  StakeTransactionProcessor(Blockchain& blockchain);

  /// Initialize storages
  void init_storages(const std::string& config_dir);

  /// Synchronize with blockchain
  void synchronize();

  typedef std::function<void(const stake_transaction_array&)> stake_transactions_update_handler;

  /// Update handler for new stake transactions
  void set_on_update_stake_transactions_handler(const stake_transactions_update_handler&);

  /// Force invoke update handler for stake transactions
  void invoke_update_stake_transactions_handler(bool force = true);

  typedef BlockchainBasedList::supernode_tier_array supernode_tier_array;
  typedef std::function<void(uint64_t block_number, const supernode_tier_array&)> blockchain_based_list_update_handler;

  /// Update handler for new blockchain based list
  void set_on_update_blockchain_based_list_handler(const blockchain_based_list_update_handler&);

  /// Force invoke update handler for blockchain based list
  void invoke_update_blockchain_based_list_handler(bool force = true, size_t depth = 1);

private:
  void process_block(uint64_t block_index, const block& block, bool update_storage = true);
  void invoke_update_stake_transactions_handler_impl();
  void invoke_update_blockchain_based_list_handler_impl(size_t depth);
  void process_block_stake_transaction(uint64_t block_index, const block& block, bool update_storage = true);
  void process_block_blockchain_based_list(uint64_t block_index, const block& block, bool update_storage = true);

private:
  Blockchain& m_blockchain;
  std::unique_ptr<StakeTransactionStorage> m_storage;
  std::unique_ptr<BlockchainBasedList> m_blockchain_based_list;
  epee::critical_section m_storage_lock;
  stake_transactions_update_handler m_on_stake_transactions_update;
  blockchain_based_list_update_handler m_on_blockchain_based_list_update;
  bool m_stake_transactions_need_update;
  bool m_blockchain_based_list_need_update;
};

}
