#include <random>
#include <vector>
#include <cstdio>

const size_t HASH_SIZE = 32;
const size_t PREVIOS_BLOCKCHAIN_BASED_LIST_MAX_SIZE = 0;
const size_t VALID_SUPERNODES_COUNT = 250;
const unsigned int EXPERIMENTS_COUNT = 100000;

struct hash
{
  char data[HASH_SIZE];
};

typedef std::vector<size_t> IndexArray;

char get_random_char()
{
  return rand();
}

void get_random_block_hash(hash& h)
{
  for (size_t i=0; i<HASH_SIZE; i++)
    h.data[i] = get_random_char();
}

size_t extract_index(const char* it, size_t length)
{
  size_t result = 0;

  for (;length--; it++)
    result = (result << 8) + size_t(*reinterpret_cast<const unsigned char*>(it));

  return result;
}

size_t log2_int(size_t value)
{
  int result = 0;

  while (value >>= 1) result++;

  return result;
}

void select_supernodes(const hash& block_hash, size_t items_count, const IndexArray& src_list, IndexArray& dst_list)
{
  if (items_count > src_list.size())
    items_count = src_list.size();

  if (!items_count)
    return;

  static const size_t block_hash_items_count = sizeof(block_hash.data) / sizeof(*block_hash.data);

  size_t step                 = log2_int(src_list.size() - 1) / 8 + 1,
         rnd_value_len        = step + 1,
         max_iterations_count = block_hash_items_count / step - 1;

  if (items_count > max_iterations_count)
    items_count = max_iterations_count;

  IndexArray list = src_list;

  auto hash_ptr = &block_hash.data[0];

  for (size_t i=0; i<items_count; i++, hash_ptr += step)
  {
    size_t base_index      = size_t((extract_index(hash_ptr, rnd_value_len) * list.size()) >> (rnd_value_len * 8)),
           supernode_index = list[base_index];

    list.erase(list.begin() + base_index);

    dst_list.push_back(supernode_index);
  }
}

void apply_block(const hash& block_hash, const IndexArray& valid_supernodes, const IndexArray& prev_supernodes, IndexArray& out_supernodes)
{
    //prepare lists of valid supernodes (stake period is valid)

  IndexArray current_supernodes = valid_supernodes;

    //sort valid supernodes

  std::sort(current_supernodes.begin(), current_supernodes.end());

    //select supernodes from the previous list

  IndexArray new_supernodes;

  select_supernodes(block_hash, PREVIOS_BLOCKCHAIN_BASED_LIST_MAX_SIZE, prev_supernodes, new_supernodes);

    //remove supernodes from prev list from current list

  for (size_t supernode_index : new_supernodes)
    current_supernodes.erase(std::remove(current_supernodes.begin(), current_supernodes.end(), supernode_index), current_supernodes.end());

    //select supernodes from the current list

  select_supernodes(block_hash, valid_supernodes.size() - new_supernodes.size(), current_supernodes, new_supernodes);

    //write results

  out_supernodes.swap(new_supernodes);
}

int main()
{
    //simulate valid supernodes candidates generation

  IndexArray valid_supernodes;

  for (size_t i=0; i<VALID_SUPERNODES_COUNT; i++)
    valid_supernodes.push_back(i);

    //make some experiments  

  IndexArray frequencies(VALID_SUPERNODES_COUNT, 0);

  IndexArray selected_supernodes;

  for (unsigned int i=0; i<EXPERIMENTS_COUNT; i++)
  {
      //generate random hash

    hash block_hash;

    get_random_block_hash(block_hash);

      //apply block

    IndexArray new_selected_supernodes;

    apply_block(block_hash, valid_supernodes, selected_supernodes, new_selected_supernodes);

    new_selected_supernodes.swap(selected_supernodes);

      //update frequencies

    for (size_t index : new_selected_supernodes)
      frequencies[index]++;
  }

    //print results

  printf("Results after %u experiments:\n", EXPERIMENTS_COUNT);

  for (unsigned int i=0; i<VALID_SUPERNODES_COUNT; i++)
    printf(" f[%03u]: %.3f\n", i, double(frequencies[i]) / EXPERIMENTS_COUNT);

  return 0;
}
