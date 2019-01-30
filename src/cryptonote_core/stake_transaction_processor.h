#pragma once

#include <functional>

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

  /// Process block
  void process_block(uint64_t block_index, const block& block, bool update_storage = true);

  /// Synchronize with blockchain
  void synchronize();

  typedef std::function<void(const stake_transaction_array&)> stake_transactions_update_handler;

  /// Update handler for new stake transactions
  void set_on_update_handler(const stake_transactions_update_handler&);

  /// Force invoke update handler
  void invoke_update_handler(bool force = true);

private:
  void invoke_update_handler_impl();
  void process_block_stake_transaction(uint64_t block_index, const block& block, bool update_storage = true);
  void process_block_blockchain_based_list(uint64_t block_index, const block& block, bool update_storage = true);

private:
  Blockchain& m_blockchain;
  StakeTransactionStorage m_storage;
  BlockchainBasedList m_blockchain_based_list;
  epee::critical_section m_storage_lock;
  stake_transactions_update_handler m_on_stake_transactions_update;
  bool m_stake_transactions_need_update;
};

}
