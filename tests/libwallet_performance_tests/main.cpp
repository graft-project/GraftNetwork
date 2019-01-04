#include "gtest/gtest.h"

#include "wallet/graft_wallet.h"

#include <atomic>
#include <memory>
#include <thread>
#include <cstdio>

using namespace tools;

namespace
{

const std::string TESTNET_DAEMON_ADDRESS = "localhost:" + std::to_string(config::testnet::RPC_DEFAULT_PORT);

void wallet_worker(std::atomic<int>& wallets_counter, int total_wallets_count)
{
  std::list<std::shared_ptr<wallet2>> wallets;

  for (;;)
  {
    int wallet_index = ++wallets_counter;

    if (total_wallets_count < wallet_index)
      break;

    try
    {
      const bool testnet = true;

      std::shared_ptr<GraftWallet> wallet = std::make_shared<GraftWallet>(testnet);

      wallet->init(TESTNET_DAEMON_ADDRESS, boost::optional<epee::net_utils::http::login>(), 0);
      wallet->set_seed_language("English");
    
      crypto::secret_key secret_key = wallet->generateFromData("");

      wallet->refresh();

      wallets.emplace_back(std::move(wallet));

      printf("Wallet #%d has been created\n", wallet_index);
      fflush(stdout);
    }
    catch (std::exception& e)
    {
      FAIL() << e.what();
    }
  }
}

}

TEST(WalletPerformanceTest, HttpConnectionPool)
{
  const int total_wallets_count = 10;
  const int threads_count = 2;

  std::vector<std::thread> threads;
  std::atomic<int> wallets_counter(0);

  for (int i=0; i<threads_count; i++)
    threads.emplace_back([&]{ wallet_worker(wallets_counter, total_wallets_count); });

  for (std::thread& thread : threads)
    if (thread.joinable())
      thread.join();
}

int main(int argc, char** argv)
{
  mlog_configure("", true);
  mlog_set_log_level(1);

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
