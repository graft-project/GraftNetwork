#pragma once

#include <functional>

#include "blockchain.h"
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
  void set_on_update_handler(const stake_transactions_update_handler&);

  /// Force invoke update handler
  void invoke_update_handler(bool force = true);

private:
  void invoke_update_handler_impl();

private:
  Blockchain& m_blockchain;
  StakeTransactionStorage m_storage;
  epee::critical_section m_storage_lock;
  stake_transactions_update_handler m_on_stake_transactions_update;
  bool m_stake_transactions_need_update;
};

}
