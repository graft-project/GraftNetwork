#include "gtest/gtest.h"

#include "wallet/wallet2.h"

#include <thread>
#include <atomic>

TEST(WalletPerformanceTest, HttpConnectionPool)
{

}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
