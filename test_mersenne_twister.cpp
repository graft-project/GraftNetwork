#include <random>
#include <vector>
#include <cstdio>

const size_t HASH_SIZE = 32;
const size_t VALID_SUPERNODES_COUNT = 250;
const unsigned int EXPERIMENTS_COUNT = 10000;

struct hash
{
  char data[HASH_SIZE];
};

typedef std::vector<size_t> IndexArray;

char get_random_char()
{
  static std::random_device random_device;
  static std::mt19937 generator(random_device());
  static std::uniform_int_distribution<char> random_char(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());

  return random_char(generator);
}

void get_random_block_hash(hash& h)
{
  for (size_t i=0; i<HASH_SIZE; i++)
    h.data[i] = get_random_char();
}

void select_supernodes(const hash& block_hash, size_t items_count, const IndexArray& src_list, IndexArray& dst_list)
{
  size_t src_list_size = src_list.size();

  if (items_count > src_list_size)
    items_count = src_list_size;

  static std::mt19937 generator;

  std::seed_seq seed(reinterpret_cast<const unsigned char*>(&block_hash.data[0]), reinterpret_cast<const unsigned char*>(&block_hash.data[HASH_SIZE]));
  std::uniform_int_distribution<size_t> random_index(0, items_count-1);

  generator.seed(seed);

  for (size_t i=0; i<items_count; i++)
  {
    size_t base_index = random_index(generator);

    for (size_t offset=0;; offset++)
    {    
      if (offset == src_list_size)
        return; //all supernodes have been selected

      size_t supernode_index = src_list[(offset + base_index) % src_list_size];
      bool already_added = false;

      for (size_t already_added_index : dst_list)
        if (already_added_index == supernode_index)
        {
          already_added = true;
          break;
        }
      
      if (already_added)
        continue; //supernode has been already selected

      dst_list.push_back(supernode_index);

      break;
    }
  }
}

void apply_block(const hash& block_hash, const IndexArray& valid_supernodes, const IndexArray& prev_supernodes, IndexArray& out_supernodes)
{
    //prepare lists of valid supernodes (stake period is valid)

  IndexArray current_supernodes = valid_supernodes;

    //sort valid supernodes

  std::sort(current_supernodes.begin(), current_supernodes.end());

    //select supernodes

  IndexArray new_supernodes;

  select_supernodes(block_hash, current_supernodes.size(), current_supernodes, new_supernodes);

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

  printf("Results after %u experiments: ", EXPERIMENTS_COUNT);
  fflush(stdout);

  for (unsigned int i=0; i<EXPERIMENTS_COUNT; i++)
  {
    if (i % 100 == 0)
    {
      printf(".");
      fflush(stdout);
    }

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
  printf("\n");

    //print results

  for (unsigned int i=0; i<VALID_SUPERNODES_COUNT; i++)
    printf(" f[%03u]: %.3f\n", i, double(frequencies[i]) / EXPERIMENTS_COUNT);

  return 0;
}
