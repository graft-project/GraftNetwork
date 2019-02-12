#include "blockchain_based_list.h"
#include "file_io_utils.h"
#include "serialization/binary_utils.h"
#include "stake_transaction_processor.h"
#include "graft_rta_config.h"

using namespace cryptonote;  

namespace
{

const size_t PREVIOS_BLOCKCHAIN_BASED_LIST_MAX_SIZE = 1; //TODO: configuration parameter
const size_t BLOCKCHAIN_BASED_LISTS_HISTORY_DEPTH   = 1000;

}

BlockchainBasedList::BlockchainBasedList(const std::string& m_storage_file_name)
  : m_storage_file_name(m_storage_file_name)
  , m_block_height()
  , m_history_depth()
  , m_need_store()
{
  load();
}

const BlockchainBasedList::supernode_tier_array& BlockchainBasedList::tiers() const
{
  if (m_history.empty())
    throw std::runtime_error("internal error: attempt to get tier from empty blockchain based list");

  return m_history.back();
}

namespace
{

bool is_valid_stake(uint64_t block_height, uint64_t stake_block_height, uint64_t stake_unlock_time)
{
  if (block_height < stake_block_height)
    return false; //stake transaction block is in future

  uint64_t stake_first_valid_block = stake_block_height + config::graft::STAKE_VALIDATION_PERIOD,
           stake_last_valid_block  = stake_block_height + stake_unlock_time + config::graft::TRUSTED_RESTAKING_PERIOD;

  if (stake_last_valid_block <= block_height)
    return false; //stake transaction is not valid

  return true;
}

size_t extract_index(const char* it, size_t length)
{
  size_t result = 0;

  for (;length--; it++)
    result = (result << 8) + size_t(*it);

  return result;
}

}

void BlockchainBasedList::select_supernodes(const crypto::hash& block_hash, size_t items_count, const supernode_array& src_list, supernode_array& dst_list)
{
  size_t src_list_size = src_list.size();

  if (items_count > src_list_size)
    items_count = src_list_size;

  static const size_t block_hash_items_count = sizeof(block_hash.data) / sizeof(*block_hash.data);

  size_t step                 = size_t(log2(double(src_list.size())) / 8) + 1,
         max_iterations_count = block_hash_items_count / step;   

  if (items_count > max_iterations_count)
    items_count = max_iterations_count;

  decltype(&block_hash.data[0]) hash_ptr = &block_hash.data[0];

  for (size_t i=0; i<items_count; i++, hash_ptr += step)
  {
    size_t base_index = extract_index(hash_ptr, step);

    for (size_t offset=0;; offset++)
    {    
      if (offset == src_list_size)
        return; //all supernodes have been selected

      size_t               supernode_index     = (offset + base_index) % src_list_size;
      const supernode& src_desc            = src_list[supernode_index];
      const std::string&   supernode_public_id = src_desc.supernode_public_id;

      if (selected_supernodes_cache.find(supernode_public_id) != selected_supernodes_cache.end())
        continue; //supernode has been already selected

      dst_list.push_back(src_desc);

      selected_supernodes_cache.insert(supernode_public_id);

      break;
    }
  }
}

void BlockchainBasedList::apply_block(uint64_t block_height, const crypto::hash& block_hash, StakeTransactionStorage& stake_txs_storage)
{
  if (block_height <= m_block_height)
    return;

  if (block_height != m_block_height + 1)
    throw std::runtime_error("block_height should be next after the block already processed");

  const StakeTransactionStorage::stake_transaction_array& stake_txs = stake_txs_storage.get_txs();

    //build blockchain based list for each tier

  supernode_array prev_supernodes, current_supernodes;
  supernode_tier_array new_tier;

  for (size_t i=0; i<config::graft::TIERS_COUNT; i++)
  {
    prev_supernodes.clear();
    current_supernodes.clear();

      //prepare lists of valid supernodes (stake period is valid)

    if (!m_history.empty())
      prev_supernodes = m_history.back()[i];

    std::remove_if(prev_supernodes.begin(), prev_supernodes.end(), [block_height](const supernode& desc) {
      return !is_valid_stake(block_height, desc.block_height, desc.unlock_time);
    });

    current_supernodes.reserve(stake_txs.size());

    for (const stake_transaction& stake_tx : stake_txs)
    {
      if (!is_valid_stake(block_height, stake_tx.block_height, stake_tx.unlock_time))
        continue;

      if (stake_tx.tier != i)
        continue;

      supernode sn;

      sn.supernode_public_id = stake_tx.supernode_public_id;
      sn.block_height        = stake_tx.block_height;
      sn.unlock_time         = stake_tx.unlock_time;

      current_supernodes.emplace_back(std::move(sn));
    }

      //sort valid supernodes by the age of stake

    std::sort(current_supernodes.begin(), current_supernodes.end(), [](const supernode& s1, const supernode& s2) { return s1.block_height < s2.block_height; });

      //select supernodes from the previous list

    selected_supernodes_cache.clear();

    supernode_array new_supernodes;
  
    select_supernodes(block_hash, PREVIOS_BLOCKCHAIN_BASED_LIST_MAX_SIZE, prev_supernodes, new_supernodes);

      //select supernodes from the current list

    select_supernodes(block_hash, current_supernodes.size() - new_supernodes.size(), current_supernodes, new_supernodes);

      //update tier

    new_tier.emplace_back(std::move(new_supernodes));
  }

    //update history

  m_history.emplace_back(std::move(new_tier));

  if (m_history_depth < BLOCKCHAIN_BASED_LISTS_HISTORY_DEPTH)
  {
    m_history_depth++;
  }
  else
  {
    m_history.pop_front();
  }

  m_block_height = block_height;
  m_need_store = true;
}

void BlockchainBasedList::remove_latest_block()
{
  if (!m_block_height)
    return;

  m_need_store = true;

  m_block_height--;
  m_history_depth--;

  m_history.pop_back();

  if (m_history.empty())
    m_block_height = 0;
}

namespace
{

struct blockchain_based_list_container
{
  uint64_t block_height;
  size_t history_depth;
  BlockchainBasedList::list_history& history;

  blockchain_based_list_container(uint64_t block_height, size_t history_depth, BlockchainBasedList::list_history& history)
    : block_height(block_height), history_depth(history_depth), history(history) {}

  BEGIN_SERIALIZE_OBJECT()
    FIELD(block_height)
    FIELD(history_depth)
    FIELD(history)
  END_SERIALIZE()
};

}

void BlockchainBasedList::store() const
{
  blockchain_based_list_container data(m_block_height, m_history_depth, const_cast<list_history&>(m_history));

  std::ofstream ostr;
  ostr.open(m_storage_file_name, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);

  binary_archive<true> oar(ostr);

  bool success = ::serialization::serialize(oar, data);

  ostr.close();

  CHECK_AND_ASSERT_THROW_MES(success && ostr.good(), "Error at save blockchain based list file '" << m_storage_file_name << "'");

  m_need_store = false;
}

void BlockchainBasedList::load()
{
  if (!boost::filesystem::exists(m_storage_file_name))
    return;

  std::string buffer;
  bool r = epee::file_io_utils::load_file_to_string(m_storage_file_name, buffer);

  CHECK_AND_ASSERT_THROW_MES(r, "blockchain based list file '" << m_storage_file_name << "' is not found");

  try
  {
    LOG_PRINT_L0("Trying to parse blockchain based list");

    list_history new_history;
    blockchain_based_list_container data(0, 0, new_history);

    r = ::serialization::parse_binary(buffer, data);

    CHECK_AND_ASSERT_THROW_MES(r, "internal error: failed to deserialize blockchain based list file '" << m_storage_file_name << "'");

    m_block_height  = data.block_height;
    m_history_depth = data.history_depth;

    std::swap(m_history, data.history);

    m_need_store = false;
  }
  catch (...)
  {
    LOG_PRINT_L0("Can't parse blockchain based list file '" << m_storage_file_name << "'");
    throw;
  }
}
