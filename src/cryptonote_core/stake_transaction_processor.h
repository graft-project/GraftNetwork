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
  static constexpr uint64_t STAKE_MAX_UNLOCK_TIME = 1000;
  static constexpr uint64_t STAKE_VALIDATION_PERIOD = 6;
  static constexpr uint64_t TRUSTED_RESTAKING_PERIOD = 6;
  //static constexpr uint64_t TRUSTED_RESTAKING_PERIOD = 10000;

  //  50,000 GRFT –  tier 1
  //  90,000 GRFT –  tier 2
  //  150,000 GRFT – tier 3
  //  250,000 GRFT – tier 4
  static constexpr uint64_t TIER1_STAKE_AMOUNT = COIN *  50000;
  static constexpr uint64_t TIER2_STAKE_AMOUNT = COIN *  90000;
  static constexpr uint64_t TIER3_STAKE_AMOUNT = COIN * 150000;
  static constexpr uint64_t TIER4_STAKE_AMOUNT = COIN * 250000;

  typedef StakeTransactionStorage::stake_transaction_array stake_transaction_array;

  StakeTransactionProcessor(Blockchain& blockchain);

  /// Process block
  void process_block(uint64_t block_index, const block& block, bool update_storage = true);

  /// Synchronize with blockchain
  void synchronize();

  typedef std::function<void(const stake_transaction_array&)> stake_transactions_update_handler;

  /// Update handler for new stake transactions
  void set_on_update_stake_transactions_handler(const stake_transactions_update_handler&);

  /// Force invoke update handler for stake transactions
  void invoke_update_stake_transactions_handler(bool force = true);

  typedef std::function<void(uint64_t block_number, const std::vector<std::string>&)> blockchain_based_list_update_handler;

  /// Update handler for new blockchain based list
  void set_on_update_blockchain_based_list_handler(const blockchain_based_list_update_handler&);

  /// Force invoke update handler for blockchain based list
  void invoke_update_blockchain_based_list_handler(bool force = true);

private:
  void invoke_update_stake_transactions_handler_impl();
  void invoke_update_blockchain_based_list_handler_impl();
  void process_block_stake_transaction(uint64_t block_index, const block& block, bool update_storage = true);
  void process_block_blockchain_based_list(uint64_t block_index, const block& block, bool update_storage = true);

private:
  Blockchain& m_blockchain;
  StakeTransactionStorage m_storage;
  BlockchainBasedList m_blockchain_based_list;
  epee::critical_section m_storage_lock;
  stake_transactions_update_handler m_on_stake_transactions_update;
  blockchain_based_list_update_handler m_on_blockchain_based_list_update;
  bool m_stake_transactions_need_update;
  bool m_blockchain_based_list_need_update;
};

}
