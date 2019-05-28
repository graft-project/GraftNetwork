#include "blockchain.h"
#include "file_io_utils.h"
#include "cryptonote_basic/account_boost_serialization.h"
#include "serialization/binary_utils.h"
#include "../graft_rta_config.h"
#include "stake_transaction_storage.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "staketransaction.storage"

using namespace cryptonote;

namespace
{

const uint64_t BLOCK_HASHES_HISTORY_DEPTH       = 1000;
const uint64_t STAKE_TRANSACTIONS_HISTORY_DEPTH = BLOCK_HASHES_HISTORY_DEPTH + config::graft::STAKE_VALIDATION_PERIOD + config::graft::TRUSTED_RESTAKING_PERIOD;

struct stake_transaction_file_data
{
  uint64_t last_processed_block_index;
  size_t last_processed_block_hashes_count;
  StakeTransactionStorage::stake_transaction_array& stake_txs;
  StakeTransactionStorage::block_hash_list& block_hashes;
  StakeTransactionStorage::disqualification_array& disqualifications;
  StakeTransactionStorage::disqualification2_storage_array& disqualifications2_storage;


  stake_transaction_file_data(uint64_t in_last_processed_block_index, StakeTransactionStorage::stake_transaction_array& in_stake_txs,
    size_t in_last_processed_block_hashes_count, StakeTransactionStorage::block_hash_list& in_block_hashes,
    StakeTransactionStorage::disqualification_array& disqualifications, StakeTransactionStorage::disqualification2_storage_array& disqualifications2_storage)
    : last_processed_block_index(in_last_processed_block_index)
    , last_processed_block_hashes_count(in_last_processed_block_hashes_count)
    , stake_txs(in_stake_txs)
    , block_hashes(in_block_hashes)
    , disqualifications(disqualifications)
    , disqualifications2_storage(disqualifications2_storage)
  {
  }

  BEGIN_SERIALIZE_OBJECT()
    FIELD(last_processed_block_index)
    FIELD(last_processed_block_hashes_count)
    FIELD(block_hashes)
    FIELD(stake_txs)
    FIELD(disqualifications)
    FIELD(disqualifications2_storage)
  END_SERIALIZE()
};

}

StakeTransactionStorage::StakeTransactionStorage(const std::string& storage_file_name, uint64_t first_block_number)
  : m_storage_file_name(storage_file_name)
  , m_last_processed_block_index(first_block_number)
  , m_last_processed_block_hashes_count()
  , m_need_store()
  , m_supernode_stakes_update_block_number()
  , m_first_block_number(first_block_number)
{
  load();
}

void StakeTransactionStorage::add_tx(const stake_transaction& tx)
{
  m_stake_txs.push_back(tx);

  m_need_store = true;
}

void StakeTransactionStorage::add_disquals(const disqualification_array& disqs)
{
  if(disqs.empty()) return;
    //sort and merge
  auto middle = m_disqualifications.insert(m_disqualifications.end(), disqs.begin(), disqs.end());
  std::sort(middle, m_disqualifications.end(), disqualification::less_id_str);
  std::inplace_merge(m_disqualifications.begin(), middle, m_disqualifications.end(), disqualification::less_id_str);

  m_need_store = true;
}

StakeTransactionStorage::disqualification2_array StakeTransactionStorage::disquals2_from_storage(const disqualification2_storage_array& disqs_store)
{
  disqualification2_array disqs;
  disqs.reserve(disqs_store.size()); //at least
  for(const auto& item_store : disqs_store)
  {
    tx_extra_graft_disqualification2 extra_d2;
    bool res = serialization::parse_binary(item_store.blob, extra_d2);
    assert(res);
    for(const auto& id : extra_d2.item.ids)
    {
      disqualification2 d2;
      d2.block_index = item_store.block_index;
      d2.id_str = epee::string_tools::pod_to_hex(id);
      disqs.emplace_back( std::move(d2) );
    }
  }
  //sort
  std::sort(disqs.begin(), disqs.end(), disqualification2::less_id_str);
  return disqs;
}

void StakeTransactionStorage::add_disquals2(const disqualification2_storage_array& disqs_store)
{
  if(disqs_store.empty()) return;
  m_disqualifications2_storage.insert(m_disqualifications2_storage.end(), disqs_store.begin(), disqs_store.end());
    //make disqualification2 array from storage items
  disqualification2_array disqs = disquals2_from_storage(disqs_store);
    //merge
  auto middle = m_disqualifications2.insert(m_disqualifications2.end(), disqs.begin(), disqs.end());
  std::inplace_merge(m_disqualifications2.begin(), middle, m_disqualifications2.end(), disqualification2::less_id_str);

  m_need_store = true;
}

bool StakeTransactionStorage::is_disqualified(uint64_t block_number, const std::string& supernode_public_id) const
{
  {//disqualification
    disqualification tmp; tmp.id_str = supernode_public_id;
    auto pair = std::equal_range(m_disqualifications.begin(), m_disqualifications.end(), tmp, disqualification::less_id_str);
    bool res = std::any_of(pair.first, pair.second, [block_number](const disqualification& v){ return v.is_active_for(block_number); });
    if(res) return true;
  }
  {//disqualification2
    disqualification2 tmp; tmp.id_str = supernode_public_id;
    auto pair = std::equal_range(m_disqualifications2.begin(), m_disqualifications2.end(), tmp, disqualification2::less_id_str);
    return std::any_of(pair.first, pair.second, [block_number](const disqualification2& v){ return v.is_active_for(block_number); });
  }
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

  m_need_store = true;

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
  if (!m_last_processed_block_hashes_count)
    return;

  m_need_store = true;

  m_stake_txs.erase(std::remove_if(m_stake_txs.begin(), m_stake_txs.end(), [&](const stake_transaction& tx) {
    return tx.block_height == m_last_processed_block_index;
  }), m_stake_txs.end());

  m_last_processed_block_hashes_count--;
  m_last_processed_block_index--;

  m_last_processed_block_hashes.pop_back();

  if (m_last_processed_block_hashes.empty())
  {
      //out of block hashes cache - restore from the beginning

    m_stake_txs.clear();

    m_last_processed_block_index = m_first_block_number;
  }
}

const StakeTransactionStorage::supernode_stake_array& StakeTransactionStorage::get_supernode_stakes(uint64_t block_number)
{
  update_supernode_stakes(block_number);
  return m_supernode_stakes;
}

const StakeTransactionStorage::supernode_disqualification_array& StakeTransactionStorage::get_supernode_disqualiications(uint64_t block_number)
{
  update_supernode_stakes(block_number);
  return m_supernode_disqualifications;
}

void StakeTransactionStorage::clear_supernode_stakes()
{
  m_supernode_stakes.clear();
  m_supernode_stake_indexes.clear();
  m_supernode_disqualifications.clear();

  m_supernode_stakes_update_block_number = 0;
}

namespace
{

unsigned int get_tier(uint64_t stake)
{
  return 0 +
    (stake >= config::graft::TIER1_STAKE_AMOUNT) +
    (stake >= config::graft::TIER2_STAKE_AMOUNT) +
    (stake >= config::graft::TIER3_STAKE_AMOUNT) +
    (stake >= config::graft::TIER4_STAKE_AMOUNT);
}

}

void StakeTransactionStorage::update_supernode_stakes(uint64_t block_number)
{
  if (block_number == m_supernode_stakes_update_block_number)
    return;

  MDEBUG("Build stakes for block " << block_number);

  m_supernode_stakes.clear();
  m_supernode_stake_indexes.clear();
  m_supernode_disqualifications.clear();

  try
  {
      //disqualifications
    for(const auto& disq : m_disqualifications)
    {
      if(!disq.is_active_for(block_number))
        continue;
      m_supernode_disqualifications.push_back(disq.blob);
    }

      //stakes
    m_supernode_stakes.reserve(m_stake_txs.size());

    for (const stake_transaction& tx : m_stake_txs)
    {
      bool obsolete_stake = false;

      if (!tx.is_valid(block_number))
      {
        uint64_t first_history_block = block_number - config::graft::SUPERNODE_HISTORY_SIZE;

        if (tx.block_height + tx.unlock_time < first_history_block)
          continue;

          //add stake transaction with zero amount to indicate correspondent node presense for search in supernode

        obsolete_stake = true;
      }

      MDEBUG("...use stake transaction " << tx.hash << " as " << (obsolete_stake ? "obsolete" : "normal") << " stake transaction ");

        //compute stake validity period

      uint64_t min_tx_block_height = tx.block_height + config::graft::STAKE_VALIDATION_PERIOD,
               max_tx_block_height = tx.block_height + tx.unlock_time + config::graft::TRUSTED_RESTAKING_PERIOD;

        //search for a stake of the corresponding supernode

      supernode_stake_index_map::iterator it = m_supernode_stake_indexes.find(tx.supernode_public_id);

      if (it == m_supernode_stake_indexes.end())
      {
          //add new supernode stake

        supernode_stake new_stake;

        if (obsolete_stake)
        {
          new_stake.amount       = 0;
          new_stake.tier         = 0;
          new_stake.block_height = 0;
          new_stake.unlock_time  = 0;
        }
        else
        {
          new_stake.amount       = tx.amount;
          new_stake.tier         = get_tier(new_stake.amount);
          new_stake.block_height = min_tx_block_height;
          new_stake.unlock_time  = max_tx_block_height - min_tx_block_height;

          MDEBUG("...first stake transaction for supernode " << tx.supernode_public_id << ": amount=" << tx.amount << ", tier=" <<
            new_stake.tier << ", validity=[" << min_tx_block_height << ";" << max_tx_block_height << ")");
        }

        new_stake.supernode_public_id      = tx.supernode_public_id;
        new_stake.supernode_public_address = tx.supernode_public_address;

        m_supernode_stakes.emplace_back(std::move(new_stake));

        m_supernode_stake_indexes[tx.supernode_public_id] = m_supernode_stakes.size() - 1;

        continue;
      }

        //update existing supernode's stake

      if (obsolete_stake)
        continue; //no need to aggregate fields from obsolete stake

      MDEBUG("...accumulate stake transaction for supernode " << tx.supernode_public_id << ": amount=" << tx.amount <<
        ", validity=[" << min_tx_block_height << ";" << max_tx_block_height << ")");

      supernode_stake& stake = m_supernode_stakes[it->second];

      if (!stake.amount)
      {
          //set fields for supernode which has been constructed for obsolete stake

        stake.amount       = tx.amount;
        stake.tier         = get_tier(stake.amount);
        stake.block_height = min_tx_block_height;
        stake.unlock_time  = max_tx_block_height - min_tx_block_height;

        continue;
      }

        //aggregate fields for existing stake

      stake.amount += tx.amount;
      stake.tier    = get_tier(stake.amount);

        //find intersection of stake transaction intervals

      uint64_t min_block_height = stake.block_height,
               max_block_height = min_block_height + stake.unlock_time;

      if (min_tx_block_height > min_block_height)
        min_block_height = min_tx_block_height;

      if (max_tx_block_height < max_block_height)
        max_block_height = max_tx_block_height;

      if (max_block_height <= min_block_height)
        max_block_height = min_block_height;

      stake.block_height = min_block_height;
      stake.unlock_time  = max_block_height - min_block_height;

      MDEBUG("...stake for supernode " << tx.supernode_public_id << ": amount=" << stake.amount << ", tier=" << stake.tier <<
        ", validity=[" << min_block_height << ";" << max_block_height << ")");
    }
  }
  catch (...)
  {
    m_supernode_stakes.clear();
    m_supernode_stake_indexes.clear();
    m_supernode_disqualifications.clear();

    m_supernode_stakes_update_block_number = 0;

    throw;
  }

  m_supernode_stakes_update_block_number = block_number;
}

const supernode_stake* StakeTransactionStorage::find_supernode_stake(uint64_t block_number, const std::string& supernode_public_id)
{
  update_supernode_stakes(block_number);

  supernode_stake_index_map::const_iterator it = m_supernode_stake_indexes.find(supernode_public_id);

  if (it == m_supernode_stake_indexes.end())
    return nullptr;

  return &m_supernode_stakes[it->second];
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
    StakeTransactionStorage::disqualification_array tmp_disqualifications;
    StakeTransactionStorage::disqualification2_storage_array tmp_disqualifications2_storage;
    stake_transaction_file_data data(0, tmp_stake_txs, 0, tmp_block_hashes, tmp_disqualifications, tmp_disqualifications2_storage);

    r = ::serialization::parse_binary(buffer, data);

    CHECK_AND_ASSERT_THROW_MES(r, "internal error: failed to deserialize stake transaction storage file '" << m_storage_file_name << "'");

    m_last_processed_block_index        = data.last_processed_block_index;
    m_last_processed_block_hashes_count = data.last_processed_block_hashes_count;

    std::swap(m_stake_txs, data.stake_txs);
    std::swap(m_last_processed_block_hashes, data.block_hashes);
    std::swap(m_disqualifications, data.disqualifications);
    std::swap(m_disqualifications2_storage, data.disqualifications2_storage);

    disqualification2_array disqs2 = disquals2_from_storage(m_disqualifications2_storage);
    m_disqualifications2.swap(disqs2);

    m_need_store = false;
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
    m_last_processed_block_hashes_count, const_cast<block_hash_list&>(m_last_processed_block_hashes)
    , const_cast<disqualification_array&>(m_disqualifications)
    , const_cast<disqualification2_storage_array&>(m_disqualifications2_storage));

  std::ofstream ostr;
  ostr.open(m_storage_file_name, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);

  binary_archive<true> oar(ostr);

  bool success = ::serialization::serialize(oar, data);

  ostr.close();
  
  CHECK_AND_ASSERT_THROW_MES(success && ostr.good(), "Error at save stake transaction storage file '" << m_storage_file_name << "'");

  m_need_store = false;
}
