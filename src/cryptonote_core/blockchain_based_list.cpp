#include "file_io_utils.h"
#include "stake_transaction_processor.h"
#include "graft_rta_config.h"
#include "blockchain_based_list.h"
#include "serialization/binary_utils.h"
#include "utils/sample_generator.h"

using namespace cryptonote;  

namespace
{

const size_t BLOCKCHAIN_BASED_LIST_SIZE = 32; //TODO: configuration parameter
const size_t PREVIOS_BLOCKCHAIN_BASED_LIST_MAX_SIZE = 16; //TODO: configuration parameter
const size_t BLOCKCHAIN_BASED_LISTS_HISTORY_DEPTH   = 1000;

}

BlockchainBasedList::BlockchainBasedList(const std::string& m_storage_file_name, uint64_t first_block_number)
  : m_storage_file_name(m_storage_file_name)
  , m_block_height(first_block_number)
  , m_history_depth()
  , m_first_block_number(first_block_number)
  , m_need_store()
{
  load();
}

const BlockchainBasedList::supernode_tier_array& BlockchainBasedList::tiers(size_t depth) const
{
  if (depth >= m_history_depth)
    throw std::runtime_error("internal error: attempt to get tier which is not present in a blockchain based list");

  if (!depth)
    return m_history.back();

  list_history::const_reverse_iterator it = m_history.rbegin();

  std::advance(it, depth);

  return *it;
}

void BlockchainBasedList::select_supernodes(size_t items_count, const supernode_array& src_list, supernode_array& dst_list)
{
  graft::generator::uniform_select(graft::generator::do_not_seed{}, items_count, src_list, dst_list);
}

void BlockchainBasedList::apply_block(uint64_t block_height, const crypto::hash& block_hash, StakeTransactionStorage& stake_txs_storage)
{
  if (block_height <= m_block_height)
    return;

  if (block_height != m_block_height + 1)
    throw std::runtime_error("block_height should be next after the block already processed");

  const StakeTransactionStorage::supernode_stake_array& stakes = stake_txs_storage.get_supernode_stakes(block_height);

    //build blockchain based list for each tier

  supernode_tier_array new_tier;

  for (size_t i=0; i<config::graft::TIERS_COUNT; i++)
  {
    supernode_array prev_supernodes;

      //prepare lists of valid supernodes for this tier

    if (!m_history.empty())
    {
      const supernode_array& full_prev_supernodes = m_history.back()[i];

      prev_supernodes.reserve(full_prev_supernodes.size());

      for (const supernode& sn : full_prev_supernodes)
      {
        const supernode_stake* stake = stake_txs_storage.find_supernode_stake(block_height, sn.supernode_public_id);

        if (!stake || !stake->amount)
          continue;

        if (stake->tier != i + 1)
          continue;

        if (stake_txs_storage.is_disqualified(block_height, sn.supernode_public_id))
          continue;

        prev_supernodes.push_back(sn);
      }
    }

    supernode_array current_supernodes;
    current_supernodes.reserve(stakes.size());

    for (const supernode_stake& stake : stakes)
    {
      if (!stake.amount)
        continue;

      if (stake.tier != i + 1)
        continue;

      supernode sn;

      sn.supernode_public_id      = stake.supernode_public_id;
      sn.supernode_public_address = stake.supernode_public_address;
      sn.amount                   = stake.amount;
      sn.block_height             = stake.block_height;
      sn.unlock_time              = stake.unlock_time;

      current_supernodes.emplace_back(std::move(sn));
    }

      //sort valid supernodes by the age of stake

    std::stable_sort(current_supernodes.begin(), current_supernodes.end(), [](const supernode& s1, const supernode& s2) {
      return s1.block_height < s2.block_height || (s1.block_height == s2.block_height && s1.supernode_public_id < s2.supernode_public_id);
    });

      //seed RNG
    graft::generator::seed_uniform_select(block_hash);
      //select supernodes from the previous list
    supernode_array new_supernodes;
    select_supernodes(PREVIOS_BLOCKCHAIN_BASED_LIST_MAX_SIZE, prev_supernodes, new_supernodes);

    if (new_supernodes.size() < BLOCKCHAIN_BASED_LIST_SIZE) //looks like it is always true
    {
        //remove supernodes of prev list from current list

      auto duplicates_filter = [&](const supernode& sn1) {
        for (const supernode& sn2 : new_supernodes)
          if (sn1.supernode_public_id == sn2.supernode_public_id)
            return true;

        return false;
      };

      current_supernodes.erase(std::remove_if(current_supernodes.begin(), current_supernodes.end(), duplicates_filter), current_supernodes.end());

        //select supernodes from the current list

      select_supernodes(BLOCKCHAIN_BASED_LIST_SIZE - new_supernodes.size(), current_supernodes, new_supernodes);
    }

      //update tier

    //LOG_PRINT_L0("Blockchain based list has been built for block " << block_height << " and tier " << i << " with " << new_supernodes.size() << " supernode(s)");

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
  if (!m_history_depth)
    return;

  m_need_store = true;

  m_block_height--;
  m_history_depth--;

  m_history.pop_back();

  if (m_history.empty())
    m_block_height = m_first_block_number;
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
