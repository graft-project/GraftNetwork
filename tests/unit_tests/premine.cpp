#include "gtest/gtest.h"

#include "cryptonote_basic/cryptonote_basic_impl.h"

using namespace cryptonote;

TEST(premine, height_1)
{
    size_t median_size = 300000;
    size_t current_block_size = 83;
    uint64_t already_generated_coins = 17592186044415;
    uint64_t reward = 0;
    uint8_t version = 6;

    bool res = get_block_reward(median_size, current_block_size, already_generated_coins, reward, version);

    ASSERT_TRUE(res);
    ASSERT_EQ(8301030000000000000, reward);
}
