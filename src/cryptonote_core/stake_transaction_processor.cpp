#include "stake_transaction_processor.h"

using namespace cryptonote;

namespace
{

const char* STAKE_TRANSACTION_STORAGE_FILE_NAME = "stake_transactions.bin";

}

StakeTransactionProcessor::StakeTransactionProcessor(Blockchain& blockchain)
  : m_blockchain(blockchain)
  , m_storage(STAKE_TRANSACTION_STORAGE_FILE_NAME)
  , m_stake_transactions_need_update(true)
{
}

void StakeTransactionProcessor::process_block(uint64_t block_index, const block& block, bool update_storage)
{
  if (block_index <= m_storage.get_last_processed_block_index())
    return;

  BlockchainDB& db = m_blockchain.get_db();

  stake_transaction stake_tx;

  for (const crypto::hash& tx_hash : block.tx_hashes)
  {
    const transaction& tx = db.get_tx(tx_hash);

    if (!get_graft_stake_tx_extra_from_extra(tx, stake_tx.supernode_public_id, stake_tx.supernode_public_address, stake_tx.supernode_signature, stake_tx.tx_secret_key))
      continue;

    stake_tx.block_height = block_index;
    stake_tx.hash = tx.hash;
    stake_tx.unlock_time = tx.unlock_time;

    m_storage.add_tx(stake_tx);

    m_stake_transactions_need_update = true;

    MCLOG(el::Level::Info, "global", "New stake transaction found at block #" << block_index << ", tx_hash=" << stake_tx.hash << ", supernode_public_id '" << stake_tx.supernode_public_id << "'");
  }

  m_storage.set_last_processed_block_index(block_index);

  if (update_storage)
    m_storage.store();
}

void StakeTransactionProcessor::synchronize()
{
  CRITICAL_REGION_LOCAL1(m_storage_lock);

  BlockchainDB& db = m_blockchain.get_db();

  uint64_t first_block_index = m_storage.get_last_processed_block_index() + 1,
           height = db.height();

  static const uint64_t SYNC_DEBUG_LOG_STEP = 10000;

  bool need_finalize_log_messages = false;
  
  for (uint64_t i=first_block_index, sync_debug_log_next_index=i + 1; i<height; i++)
  {
    if (i == sync_debug_log_next_index)
    {
      MCLOG(el::Level::Info, "global", "Stake transactions sync " << i << "/" << height);

      need_finalize_log_messages = true;
      sync_debug_log_next_index  = i + SYNC_DEBUG_LOG_STEP;

      if (sync_debug_log_next_index >= height)
        sync_debug_log_next_index = height - 1;
    }

    crypto::hash block_hash = db.get_block_hash_from_height(i);
    const block& block      = db.get_block(block_hash);

    process_block(i, block, false);
  }

  m_storage.store();

  if (m_stake_transactions_need_update && m_on_stake_transactions_update)
  {
    invoke_update_handler_impl();
    m_stake_transactions_need_update = false;
  }

  if (need_finalize_log_messages)
    MCLOG(el::Level::Info, "global", "Stake transactions sync OK");
}

void StakeTransactionProcessor::set_on_update_handler(const stake_transactions_update_handler& handler)
{
  CRITICAL_REGION_LOCAL1(m_storage_lock);
  m_on_stake_transactions_update = handler;
}

void StakeTransactionProcessor::invoke_update_handler_impl()
{
  try
  {
    m_on_stake_transactions_update(m_storage.get_txs());
  }
  catch (std::exception& e)
  {
    MCLOG(el::Level::Error, "global", "exception in StakeTransactionProcessor update handler: " << e.what());
  }
}

void StakeTransactionProcessor::invoke_update_handler(bool force)
{
  CRITICAL_REGION_LOCAL1(m_storage_lock);

  if (!m_on_stake_transactions_update)
    return;
  
  if (!m_stake_transactions_need_update && !force)
    return;

  invoke_update_handler_impl();
}
