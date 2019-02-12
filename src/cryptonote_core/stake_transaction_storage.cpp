#include "stake_transaction_storage.h"
#include "file_io_utils.h"
#include "serialization/binary_utils.h"
#include "cryptonote_basic/account_boost_serialization.h"

using namespace cryptonote;

namespace
{

const uint64_t BLOCK_HASHES_HISTORY_DEPTH = 1000;

struct stake_transaction_file_data
{
  uint64_t last_processed_block_index;
  size_t last_processed_block_hashes_count;
  StakeTransactionStorage::stake_transaction_array& stake_txs;
  StakeTransactionStorage::block_hash_list& block_hashes;

  stake_transaction_file_data(uint64_t in_last_processed_block_index, StakeTransactionStorage::stake_transaction_array& in_stake_txs,
    size_t in_last_processed_block_hashes_count, StakeTransactionStorage::block_hash_list& in_block_hashes)
    : last_processed_block_index(in_last_processed_block_index)
    , last_processed_block_hashes_count(in_last_processed_block_hashes_count)
    , stake_txs(in_stake_txs)
    , block_hashes(in_block_hashes)
  {
  }

  BEGIN_SERIALIZE_OBJECT()
    FIELD(last_processed_block_index)
    FIELD(last_processed_block_hashes_count)
    FIELD(block_hashes)
    FIELD(stake_txs)
  END_SERIALIZE()
};

}

StakeTransactionStorage::StakeTransactionStorage(const std::string& storage_file_name)
  : m_storage_file_name(storage_file_name)
  , m_last_processed_block_index()
  , m_last_processed_block_hashes_count()
{
  load();
}

void StakeTransactionStorage::add_tx(const stake_transaction& tx)
{
  m_stake_txs.push_back(tx);
}

const crypto::hash& StakeTransactionStorage::get_last_processed_block_hash() const
{
  if (m_last_processed_block_hashes.empty())
    throw std::runtime_error("internal error: can't get block hash from empty array");

  return m_last_processed_block_hashes.back();
}

void StakeTransactionStorage::add_last_processed_block(uint64_t index, const crypto::hash& hash)
{
  if (index != m_last_processed_block_index + 1)
    throw std::runtime_error("internal error: new block index must be compared to the already processed block index");

  m_last_processed_block_hashes.push_back(hash);

  if (m_last_processed_block_hashes_count < BLOCK_HASHES_HISTORY_DEPTH)
  {
    m_last_processed_block_hashes_count++;
  }
  else
  {
    m_last_processed_block_hashes.pop_front();
  }

  m_last_processed_block_index = index;
}

void StakeTransactionStorage::remove_last_processed_block()
{
  if (!m_last_processed_block_index)
    return;

  std::remove_if(m_stake_txs.begin(), m_stake_txs.end(), [&](const stake_transaction& tx) {
    return tx.block_height == m_last_processed_block_index;
  });

  m_last_processed_block_hashes_count--;
  m_last_processed_block_index--;

  m_last_processed_block_hashes.pop_back();

  if (m_last_processed_block_hashes.empty())
  {
      //out of block hashes cache - restore from the beginning

    m_stake_txs.clear();

    m_last_processed_block_index = 0;

    return;
  }
}

void StakeTransactionStorage::load()
{
  if (!boost::filesystem::exists(m_storage_file_name))
    return;

  std::string buffer;
  bool r = epee::file_io_utils::load_file_to_string(m_storage_file_name, buffer);

  CHECK_AND_ASSERT_THROW_MES(r, "stake transaction storage file '" << m_storage_file_name << "' is not found");

  try
  {
    LOG_PRINT_L0("Trying to parse stake transaction file");

    StakeTransactionStorage::stake_transaction_array tmp_stake_txs;
    StakeTransactionStorage::block_hash_list tmp_block_hashes;
    stake_transaction_file_data data(0, tmp_stake_txs, 0, tmp_block_hashes);

    r = ::serialization::parse_binary(buffer, data);

    CHECK_AND_ASSERT_THROW_MES(r, "internal error: failed to deserialize stake transaction storage file '" << m_storage_file_name << "'");

    m_last_processed_block_index        = data.last_processed_block_index;
    m_last_processed_block_hashes_count = data.last_processed_block_hashes_count;

    std::swap(m_stake_txs, data.stake_txs);
    std::swap(m_last_processed_block_hashes, data.block_hashes);
  }
  catch (...)
  {
    LOG_PRINT_L0("Can't parse stake transaction storage file '" << m_storage_file_name << "'");
    throw;
  }
}

void StakeTransactionStorage::store() const
{
  stake_transaction_file_data data(m_last_processed_block_index, const_cast<stake_transaction_array&>(m_stake_txs),
    m_last_processed_block_hashes_count, const_cast<block_hash_list&>(m_last_processed_block_hashes));

  std::ofstream ostr;
  ostr.open(m_storage_file_name, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);

  binary_archive<true> oar(ostr);

  bool success = ::serialization::serialize(oar, data);

  ostr.close();
  
  CHECK_AND_ASSERT_THROW_MES(success && ostr.good(), "Error at save stake transaction storage file '" << m_storage_file_name << "'");
}
