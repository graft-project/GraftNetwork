#include "stake_transaction_processor.h"

using namespace cryptonote;

namespace
{

const char* STAKE_TRANSACTION_STORAGE_FILE_NAME = "stake_transactions.bin";

unsigned int get_tier(uint64_t stake)
{
  return 0 +
    (stake >= StakeTransactionProcessor::TIER1_STAKE_AMOUNT) +
    (stake >= StakeTransactionProcessor::TIER2_STAKE_AMOUNT) +
    (stake >= StakeTransactionProcessor::TIER3_STAKE_AMOUNT) +
    (stake >= StakeTransactionProcessor::TIER4_STAKE_AMOUNT);
}

}

bool stake_transaction::is_valid(uint64_t block_index) const
{
  uint64_t stake_first_valid_block = block_height + StakeTransactionProcessor::STAKE_VALIDATION_PERIOD,
           stake_last_valid_block  = block_height + unlock_time + StakeTransactionProcessor::TRUSTED_RESTAKING_PERIOD;

  if (block_index < stake_first_valid_block)
    return false;

  if (block_index > stake_last_valid_block)
    return false;

  return true;
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

    uint64_t unlock_time = tx.unlock_time - block_index;

    if (unlock_time > STAKE_MAX_UNLOCK_TIME)
    {
      MCLOG(el::Level::Warning, "global", "Ignore stake transaction at block #" << block_index << ", tx_hash=" << stake_tx.hash << ", supernode_public_id '" << stake_tx.supernode_public_id << "'"
        " because unlock time " << unlock_time << " is greater than maximum allowed " << STAKE_MAX_UNLOCK_TIME);
      continue;
    }

    uint64_t amount = 0;

    for (const tx_out& out : tx.vout)
      amount += out.amount;

    stake_tx.amount = amount;
    stake_tx.block_height = block_index;
    stake_tx.hash = tx.hash;
    stake_tx.unlock_time = unlock_time;
    stake_tx.tier = get_tier(stake_tx.amount);

    m_storage.add_tx(stake_tx);

    m_stake_transactions_need_update = true;

    MCLOG(el::Level::Info, "global", "New stake transaction found at block #" << block_index << ", tx_hash=" << stake_tx.hash << ", supernode_public_id '" << stake_tx.supernode_public_id << "'");
  }

  m_storage.add_last_processed_block(block_index, db.get_block_hash_from_height(block_index));

  if (update_storage)
    m_storage.store();
}

void StakeTransactionProcessor::synchronize()
{
  CRITICAL_REGION_LOCAL1(m_storage_lock);

  BlockchainDB& db = m_blockchain.get_db();

    //unroll already processed blocks for alternative chains
  
  for (;;)
  {
    size_t   stake_tx_count = m_storage.get_tx_count();
    uint64_t last_processed_block_index = m_storage.get_last_processed_block_index();

    if (!last_processed_block_index)
      break;
    
    const crypto::hash& last_processed_block_hash  = m_storage.get_last_processed_block_hash();
    crypto::hash        last_blockchain_block_hash = db.get_block_hash_from_height(last_processed_block_index);

    if (!memcmp(&last_processed_block_hash.data[0], &last_blockchain_block_hash.data[0], sizeof(last_blockchain_block_hash.data)))
      break; //latest block hash is the same as processed

    MCLOG(el::Level::Info, "global", "Stake transactions processing: unroll block " << last_processed_block_index);

    m_storage.remove_last_processed_block();

    if (stake_tx_count != m_storage.get_tx_count())
      m_stake_transactions_need_update = true;
  }

    //apply new blocks

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
    stake_transaction_array valid_stake_txs;
    const stake_transaction_array& stake_txs = m_storage.get_txs();

    valid_stake_txs.reserve(stake_txs.size());

    uint64_t top_block_index = m_blockchain.get_db().height() - 1;

    for (const stake_transaction& tx : stake_txs)
    {
      if (!tx.is_valid(top_block_index))
        continue;

      valid_stake_txs.push_back(tx);
    }

    m_on_stake_transactions_update(valid_stake_txs);
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
