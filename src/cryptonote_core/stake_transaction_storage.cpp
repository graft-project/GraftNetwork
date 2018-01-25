#include "stake_transaction_storage.h"
#include "file_io_utils.h"
#include "serialization/binary_utils.h"
#include "cryptonote_basic/account_boost_serialization.h"

using namespace cryptonote;

namespace
{

struct stake_transaction_file_data
{
  uint64_t last_processed_block_index;
  StakeTransactionStorage::stake_transaction_array& stake_txs;

  stake_transaction_file_data(uint64_t in_last_processed_block_index, StakeTransactionStorage::stake_transaction_array& in_stake_txs)
    : last_processed_block_index(in_last_processed_block_index)
    , stake_txs(in_stake_txs)
  {
  }

  BEGIN_SERIALIZE_OBJECT()
    FIELD(last_processed_block_index)
    FIELD(stake_txs)
  END_SERIALIZE()
};

}

StakeTransactionStorage::StakeTransactionStorage(const std::string& storage_file_name)
  : m_storage_file_name(storage_file_name)
  , m_last_processed_block_index()
{
  load();
}

void StakeTransactionStorage::add_tx(const stake_transaction& tx)
{
  m_stake_txs.push_back(tx);
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
    stake_transaction_file_data data(0, tmp_stake_txs);

    r = ::serialization::parse_binary(buffer, data);

    CHECK_AND_ASSERT_THROW_MES(r, "internal error: failed to deserialize stake transaction storage file '" << m_storage_file_name << "'");

    m_last_processed_block_index = data.last_processed_block_index;
    std::swap(m_stake_txs, data.stake_txs);
  }
  catch (...)
  {
    LOG_PRINT_L0("Can't parse stake transaction storage file '" << m_storage_file_name << "'");
    throw;
  }
}

void StakeTransactionStorage::store() const
{
  stake_transaction_file_data data(m_last_processed_block_index, const_cast<stake_transaction_array&>(m_stake_txs));

  std::ofstream ostr;
  ostr.open(m_storage_file_name, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);

  binary_archive<true> oar(ostr);

  bool success = ::serialization::serialize(oar, data);

  ostr.close();
  
  CHECK_AND_ASSERT_THROW_MES(success && ostr.good(), "Error at save stake transaction storage file '" << m_storage_file_name << "'");
}
