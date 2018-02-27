// Copyright (c) 2017, The Graft Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2014-2017 The Monero Project

#include "gtest/gtest.h"

#include "wallet/wallet2_api.h"
#include "wallet/wallet2.h"

#include "include_base_utils.h"
#include "cryptonote_config.h"

#include <boost/chrono/chrono.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <iostream>
#include <vector>
#include <atomic>
#include <functional>
#include <string>
#include <chrono>
#include <thread>



using namespace tools;

static const uint64_t AMOUNT_10_GRF = 10 * COIN;

struct GraftSplittedFeeTest : public testing::Test
{
    std::string wallet_account1;
    std::string wallet_account2;
    std::string wallet_root_path;
    std::string bdb_path;
    const std::string DAEMON_ADDR = "localhost:28281";
    // miner_wallet
    std::string RECIPIENT_ADDR = "F7qe3cg9LxrTFgNGsM9fpS3DYf1UnGNAxUgc7aHyEAAXbyfSbfqzhbH7mzGzAK7vsj3PoETpuqsuMiQcCWeaC61cHkQNz72";

    GraftSplittedFeeTest()
    {
//        wallet *wallet1 = new GraftWallet(true, false);
//        e *wallet2 = new GraftWallet(true, false);
        wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
//        string wallet_path1 = wallet_root_path + "/miner_wallet";
//        string wallet_path2 = wallet_root_path + "/stake_wallet";
//        wallet1->load(wallet_path1, "");
//        wallet2->load(wallet_path2, "");
//        RECIPIENT_ADDR = wallet2->address();
////        // serialize test wallets
////        wallet_account1 = wallet1->store_keys_graft("", false);
//        delete wallet1;
////        wallet_account2 = wallet2->store_keys_graft("", false);
//        delete wallet2;
    }

    ~GraftSplittedFeeTest()
    {

    }

};

TEST_F(GraftSplittedFeeTest, SplitFeeTest)
{

    vector<string> auth_sample = {
      "F6mnCC9RwgY1bCzPuE9UB8bqxCpyo7sRfWr2uddCy3TVF3vf6CR2SPe9jgVVLCe5c3QTZ7Pkmcf8kXZ2iqH64WREQWu7wTk",
      "F6KTXGAfqa4MXLTUkSpNfzVoi5U2gzippJdLyZxaHTH8TGm4G5xjRFdSKnsm3HjYKpiHLL7SGc4k2djVP4tVtacn5x61ir7",
      "F4hqq8sZVy68uo7MzDcTAnamcPX29ZTUseE4oaPMr5DbFr4CjZcESfBW2KYpEzEtL8AV8rqp7WdkaZEDmK8pP7GBExwtABY",
      "FAY4L4HH9uJEokW3AB6rD5GSA8hw9PkNXMcUeKYf7zUh2kNtzan3m7iJrP743cfEMtMcrToW2R3NUhBaoULHWcJT9NQGJzN",
      "FBHvywDasez21pTwXqadPBNutDa5dMbnJ4mzyHxSSPD3PwMssZQCZUy5FFriXkM7Y2coofdgQdDdLNYzoVYZzTVuPTLSrHp",
      "F6c5F67DWduGAE3hmjwjhs8JuvMsVNykqME2Y1X4txdLaV4df9jqodtNbSTwkxUDWZ9s1LaAyBhLuYGvQtoSSLtUHcGRGMc",
      "F8TugaUuGqXNzg4d9AZJkz1sdhMaEGvRM5LE2KEpVQM9e2eSK4iqX5TWAtcyM35ZWdFTMdGHDJ4n9eHMKW9F8vHr1LZ7ymY",
      "F6eChDACDPebQZsKocqqrdd91QcVqUQetLaSG3fodpuqBH6bV7aMPPxLpG2jQpp2j6hJECdiBaBroApnHTgUMBMG3vctxeC"
    };

    tools::wallet2 *wallet = new tools::wallet2(true, false);
    string wallet_path1 = wallet_root_path + "/stake_wallet";
    ASSERT_NO_THROW(wallet->load(wallet_path1, ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    wallet->refresh();
    wallet->store();
    LOG_PRINT_L0("wallet balance: " <<  cryptonote::print_money(wallet->unlocked_balance()));
    LOG_PRINT_L0("wallet default mixin: " <<  wallet->default_mixin());

    vector<uint8_t> extra;
    uint64_t recipient_amount, auth_sample_amount_per_destination;
    uint64_t amount_to_send = AMOUNT_10_GRF;
    // fee 0.01
    {
      double fee_percentage = 0.01;
      vector<wallet2::pending_tx> ptxv = wallet->create_transactions_graft(RECIPIENT_ADDR, auth_sample, amount_to_send,
                                                                           fee_percentage, 0, 2, extra, true,
                                                                           recipient_amount, auth_sample_amount_per_destination);
      ASSERT_TRUE(ptxv.size() == 1);
      const wallet2::pending_tx ptx = ptxv[0];
      uint64_t amount = 0;


      for (const auto &dst : ptx.dests) {
        amount += dst.amount;
      }


      ASSERT_TRUE(amount == amount_to_send);
      ASSERT_TRUE(recipient_amount + auth_sample_amount_per_destination * auth_sample.size() == amount_to_send);
    }

    // fee 99.99
    {
      double fee_percentage = 0.01;
      vector<wallet2::pending_tx> ptxv = wallet->create_transactions_graft(RECIPIENT_ADDR, auth_sample, amount_to_send,
                                                                           fee_percentage, 0, 2, extra, true,
                                                                           recipient_amount, auth_sample_amount_per_destination);
      ASSERT_TRUE(ptxv.size() == 1);
      const wallet2::pending_tx ptx = ptxv[0];
      uint64_t amount = 0;


      for (const auto &dst : ptx.dests) {
        amount += dst.amount;
      }


      ASSERT_TRUE(amount == amount_to_send);
      ASSERT_TRUE(recipient_amount + auth_sample_amount_per_destination * auth_sample.size() == amount_to_send);
    }


    // fee 1.123
    {
      double fee_percentage = 1.123;
      vector<wallet2::pending_tx> ptxv = wallet->create_transactions_graft(RECIPIENT_ADDR, auth_sample, amount_to_send,
                                                                           fee_percentage, 0, 2, extra, true,
                                                                           recipient_amount, auth_sample_amount_per_destination);
      ASSERT_TRUE(ptxv.size() == 1);
      const wallet2::pending_tx ptx = ptxv[0];
      uint64_t amount = 0;


      for (const auto &dst : ptx.dests) {
        amount += dst.amount;
      }


      ASSERT_TRUE(amount == amount_to_send);
      ASSERT_TRUE(recipient_amount + auth_sample_amount_per_destination * auth_sample.size() == amount_to_send);
    }

}

