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
#include "supernode/graft_wallet.h"
#include "wallet/api/wallet2_api.h"
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



using namespace supernode;
using namespace tools;
using namespace Monero;
using namespace std;


struct GraftWalletTest : public testing::Test
{
    std::string wallet_account1;
    std::string wallet_account2;
    std::string wallet_root_path;
    std::string bdb_path;
    const std::string DAEMON_ADDR = "localhost:28281";


    GraftWalletTest()
    {
        GraftWallet * wallet1 = new GraftWallet(cryptonote::TESTNET, false);
        GraftWallet *wallet2 = new GraftWallet(cryptonote::TESTNET, false);
        wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
        string wallet_path1 = wallet_root_path + "/miner_wallet";
        string wallet_path2 = wallet_root_path + "/stake_wallet";
        wallet1->load(wallet_path1, "");

        wallet2->load(wallet_path2, "");
        std::cout << "test wallets loaded...\n";
        // serialize test wallets
        wallet_account1 = wallet1->store_keys_graft("", false);
        delete wallet1;
        wallet_account2 = wallet2->store_keys_graft("", false);
        delete wallet2;
    }

    ~GraftWalletTest()
    {
    }

};


TEST_F(GraftWalletTest, StoreAndLoadCache)
{
    GraftWallet *wallet = new GraftWallet(cryptonote::TESTNET, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    wallet->refresh(wallet->is_trusted_daemon());

    // store the cache
    boost::filesystem::path temp = boost::filesystem::temp_directory_path();
    temp /= boost::filesystem::unique_path();
    const string cache_filename = temp.native();
    ASSERT_NO_THROW(wallet->store_cache(cache_filename));
    delete wallet;

    boost::system::error_code ignored_ec;
    ASSERT_TRUE(boost::filesystem::exists(cache_filename, ignored_ec));
    // creating new wallet from serialized keys
    wallet = new GraftWallet(cryptonote::TESTNET, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", cache_filename));
    // check if we loaded blocks from cache
    ASSERT_TRUE(wallet->get_blockchain_current_height() > 100);
    std::cout << "cache stored to: " << cache_filename << std::endl;
    boost::filesystem::remove(temp);
    delete wallet;
}

TEST_F(GraftWalletTest, LoadWrongCache)
{
    GraftWallet *wallet = new GraftWallet(cryptonote::TESTNET, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    wallet->refresh(wallet->is_trusted_daemon());

    // store the cache
    boost::filesystem::path temp = boost::filesystem::temp_directory_path();
    temp /= boost::filesystem::unique_path();
    const string cache_filename = temp.native();
    ASSERT_NO_THROW(wallet->store_cache(cache_filename));
    delete wallet;

    boost::system::error_code ignored_ec;
    ASSERT_TRUE(boost::filesystem::exists(cache_filename, ignored_ec));
    // creating new wallet object, try to load cache from different one
    wallet = new GraftWallet(cryptonote::TESTNET, false);
    ASSERT_ANY_THROW(wallet->load_graft(wallet_account2, "", cache_filename));
    boost::filesystem::remove(temp);
    delete wallet;
}

// implemented here; normally we need the same for wallet/wallet2.cpp
TEST_F(GraftWalletTest, UseForkRule)
{
    GraftWallet *wallet = new GraftWallet(cryptonote::TESTNET, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    ASSERT_TRUE(wallet->use_fork_rules(2, 0));
    ASSERT_TRUE(wallet->use_fork_rules(4, 0));
    ASSERT_TRUE(wallet->use_fork_rules(5, 0));
    ASSERT_TRUE(wallet->use_fork_rules(6, 0));
    // this will fail on rta testnet as we need to update block version there
    ASSERT_TRUE(wallet->use_fork_rules(7, 0));
    ASSERT_FALSE(wallet->use_fork_rules(8, 0));
    ASSERT_TRUE(wallet->use_fork_rules(2, 10));
    ASSERT_TRUE(wallet->use_fork_rules(4, 10));
    ASSERT_TRUE(wallet->use_fork_rules(5, 10));
    ASSERT_TRUE(wallet->use_fork_rules(6, 10));
    // this will fail on rta testnet as we need to update block version there
    ASSERT_TRUE(wallet->use_fork_rules(7, 10));
    ASSERT_FALSE(wallet->use_fork_rules(8, 10));
}

