#include "blockchain_based_list.h"
#include "file_io_utils.h"
#include "serialization/binary_utils.h"

using namespace cryptonote;  

static const size_t   PREVIOS_BLOCKCHAIN_BASED_LIST_MAX_SIZE = 1; //TODO: configuration parameter
//static const uint64_t TRUSTED_RESTAKING_PERIOD = 100000; // TODO: configuration parameter
static const uint64_t TRUSTED_RESTAKING_PERIOD = 6; // TODO: configuration parameter
static const uint64_t STAKE_VALIDATION_PERIOD = 6; //TODO: configuration paramter

BlockchainBasedList::BlockchainBasedList(const std::string& m_storage_file_name)
  : m_storage_file_name(m_storage_file_name)
  , m_block_height()
  , m_need_store()
{
  load();
}

namespace
{

bool is_valid_stake(uint64_t block_height, uint64_t stake_block_height, uint64_t stake_unlock_time)
{
  if (block_height < stake_block_height)
    return false; //stake transaction block is in future

  uint64_t stake_first_valid_block = stake_block_height + STAKE_VALIDATION_PERIOD,
           stake_last_valid_block  = stake_block_height + stake_unlock_time + TRUSTED_RESTAKING_PERIOD;

  if (stake_last_valid_block <= block_height)
    return false; //stake transaction is not valid

  return true;
}

}

void BlockchainBasedList::select_supernodes(const crypto::hash& block_hash, size_t items_count, const SupernodeList& src_list, SupernodeList& dst_list)
{
  size_t src_list_size = src_list.size();

  if (items_count > src_list_size)
    items_count = src_list_size;

  static const size_t block_hash_items_count = sizeof(block_hash.data) / sizeof(*block_hash.data);

  if (items_count > block_hash_items_count)
    items_count = block_hash_items_count;

  decltype(&block_hash.data[0]) hash_ptr = &block_hash.data[0];

  for (size_t i=0; i<items_count; i++, hash_ptr++)
  {
    for (size_t offset=0;; offset++)
    {    
      if (offset == src_list_size)
        return; //all supernodes have been selected

      size_t               supernode_index     = (offset + size_t(*hash_ptr)) % src_list_size;
      const SupernodeDesc& src_desc            = src_list[supernode_index];
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

    //prepare lists of valid supernodes (stake period is valid)

  const StakeTransactionStorage::stake_transaction_array& stake_txs = stake_txs_storage.get_txs();

  SupernodeList prev_supernodes = m_supernodes;

  std::remove_if(prev_supernodes.begin(), prev_supernodes.end(), [block_height](const SupernodeDesc& desc) {
    return !is_valid_stake(block_height, desc.block_height, desc.unlock_time);
  });

  SupernodeList current_supernodes;

  current_supernodes.reserve(stake_txs.size());

  for (const stake_transaction& stake_tx : stake_txs)
  {
    if (!is_valid_stake(block_height, stake_tx.block_height, stake_tx.unlock_time))
      continue;

    SupernodeDesc desc;

    desc.supernode_public_id = stake_tx.supernode_public_id;
    desc.block_height        = stake_tx.block_height;
    desc.unlock_time         = stake_tx.unlock_time;

    current_supernodes.push_back(desc);
  }

    //sort valid supernodes by the age of stake

  std::sort(current_supernodes.begin(), current_supernodes.end(), [](const SupernodeDesc& s1, const SupernodeDesc& s2) { return s1.block_height < s2.block_height; });

    //select supernodes from the previous list

  selected_supernodes_cache.clear();

  SupernodeList new_supernodes;

  select_supernodes(block_hash, PREVIOS_BLOCKCHAIN_BASED_LIST_MAX_SIZE, prev_supernodes, new_supernodes);

    //select supernodes from the current list

  select_supernodes(block_hash, current_supernodes.size() - new_supernodes.size(), current_supernodes, new_supernodes);
  
    //update cached values

  std::swap(new_supernodes, m_supernodes);

  m_block_height = block_height;
  m_need_store = true;
}

namespace
{

struct blockchain_based_list_container
{
  uint64_t block_height;
  BlockchainBasedList::SupernodeList& supernodes;

  blockchain_based_list_container(uint64_t in_block_height, BlockchainBasedList::SupernodeList& in_supernodes)
    : block_height(in_block_height), supernodes(in_supernodes) {}

  BEGIN_SERIALIZE_OBJECT()
    FIELD(block_height)
    FIELD(supernodes)
  END_SERIALIZE()
};

}

void BlockchainBasedList::store() const
{
  blockchain_based_list_container data(m_block_height, const_cast<SupernodeList&>(m_supernodes));

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

    SupernodeList new_supernodes;
    blockchain_based_list_container data(0, new_supernodes);

    r = ::serialization::parse_binary(buffer, data);

    CHECK_AND_ASSERT_THROW_MES(r, "internal error: failed to deserialize blockchain based list file '" << m_storage_file_name << "'");

    m_block_height = data.block_height;
    std::swap(m_supernodes, data.supernodes);

    m_need_store = false;
  }
  catch (...)
  {
    LOG_PRINT_L0("Can't parse blockchain based list file '" << m_storage_file_name << "'");
    throw;
  }
}
