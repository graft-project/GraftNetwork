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
#include "supernode/FSN_Servant.h"
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
using namespace std;

namespace consts {

}

struct FSNServantTest : public testing::Test
{

    FSN_Servant * fsns = nullptr;
    string db_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_blockchain";


    FSNServantTest()
    {
        fsns = new FSN_Servant(db_path, "localhost:28281", "", true);

    }

    ~FSNServantTest()
    {
        delete fsns;
    }
};


TEST_F(FSNServantTest, CreateDestroyInstance)
{
    ASSERT_TRUE(fsns);
}

TEST_F(FSNServantTest, ProofOfStakeTestMiner)
{
    // miner wallet1
    const string address1 = "T6T2LeLmi6hf58g7MeTA8i4rdbVY8WngXBK3oWS7pjjq9qPbcze1gvV32x7GaHx8uWHQGNFBy1JCY1qBofv56Vwb26Xr998SE";
    const string viewkey1 = "0ae7176e5332974de64713c329d406956e8ff2fd60c85e7ee6d8c88318111007";
    // miner wallet2
    const string address2 = "T6SgjB6ps9Z5cizMGGaLvo5SbyW7eoqV4es7V73oPPPuKJVPtrtBueX1pM62zezfev7DwEUKHN8UZ8kE6fgVc4X32JWVErmSD";
    const string viewkey2 = "f5fc5db98492ee75964d81f2ec313a567fea73f57ed9b31d9085f42055798d07";

    fsns->AddFsnAccount(boost::make_shared<FSN_Data>(FSN_WalletData{"", ""}, FSN_WalletData{address1, viewkey1}));
    fsns->AddFsnAccount(boost::make_shared<FSN_Data>(FSN_WalletData{"", ""}, FSN_WalletData{address2, viewkey2}));

    vector<pair<uint64_t, boost::shared_ptr<FSN_Data>>> output =
            fsns->LastBlocksResolvedByFSN(1, 1);
    std::cout << "size: " << output.size() << std::endl;
    ASSERT_TRUE(output.size() == 1);
    ASSERT_TRUE(output[0].first == 1);
    ASSERT_TRUE(output[0].second->Miner.Addr == address1);
    output.clear();
    output = fsns->LastBlocksResolvedByFSN(1, 10);

    ASSERT_TRUE(output.size() == 10);

    ASSERT_TRUE(output[0].first == 10);
    ASSERT_TRUE(output[1].first == 9);
    ASSERT_TRUE(output[2].first == 8);
    ASSERT_TRUE(output[3].first == 7);
    ASSERT_TRUE(output[4].first == 6);
    ASSERT_TRUE(output[5].first == 5);
    ASSERT_TRUE(output[6].first == 4);
    ASSERT_TRUE(output[7].first == 3);
    ASSERT_TRUE(output[8].first == 2);
    ASSERT_TRUE(output[9].first == 1);


    ASSERT_TRUE(output[0].second->Miner.Addr == address2);
    ASSERT_TRUE(output[9].second->Miner.Addr == address1);

    output = fsns->LastBlocksResolvedByFSN(2000000, 10);
    ASSERT_TRUE(output.empty());

    output = fsns->LastBlocksResolvedByFSN(20, 10000);
    ASSERT_FALSE(output.empty());

    for (const auto &iter : output) {
        ASSERT_TRUE(iter.second->Miner.Addr == address1 || iter.second->Miner.Addr == address2);
    }
}

TEST_F(FSNServantTest, SetStakeAndMinerWallets)
{
    string wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
    bool set_failed = false;
    try {
        fsns->Set(wallet_root_path + "/stake_wallet", "", wallet_root_path + "/miner_wallet", "");
    } catch (...) {
        set_failed = true;
    }
    ASSERT_FALSE(set_failed);
    ASSERT_TRUE(fsns->GetMyMinerWallet().Addr == "T6T2LeLmi6hf58g7MeTA8i4rdbVY8WngXBK3oWS7pjjq9qPbcze1gvV32x7GaHx8uWHQGNFBy1JCY1qBofv56Vwb26Xr998SE");
    ASSERT_TRUE(fsns->GetMyMinerWallet().ViewKey == "0ae7176e5332974de64713c329d406956e8ff2fd60c85e7ee6d8c88318111007");
    ASSERT_TRUE(fsns->GetMyStakeWallet().Addr == "T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa");
    ASSERT_TRUE(fsns->GetMyStakeWallet().ViewKey == "8c0ccff03e9f2a9805e200f887731129495ff793dc678db6c5b53df814084f04");
}

TEST_F(FSNServantTest, SignAndVerify)
{
    string wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
    fsns->Set(wallet_root_path + "/stake_wallet", "", wallet_root_path + "/miner_wallet", "");

    std::string message = "Hello, Graft";
    std::string address = "T6T2LeLmi6hf58g7MeTA8i4rdbVY8WngXBK3oWS7pjjq9qPbcze1gvV32x7GaHx8uWHQGNFBy1JCY1qBofv56Vwb26Xr998SE";

    std::string signature = fsns->SignByWalletPrivateKey(message, address);
    std::cout << "signature: " << signature << std::endl;

    ASSERT_TRUE(fsns->IsSignValid(message, address, signature));
    ASSERT_FALSE(fsns->IsSignValid(message + ".", address, signature));
}


TEST_F(FSNServantTest, GetBalance1)
{
    FSN_WalletData wallet1("T6T2LeLmi6hf58g7MeTA8i4rdbVY8WngXBK3oWS7pjjq9qPbcze1gvV32x7GaHx8uWHQGNFBy1JCY1qBofv56Vwb26Xr998SE",
                           "0ae7176e5332974de64713c329d406956e8ff2fd60c85e7ee6d8c88318111007");

    uint64_t balance_10block = fsns->GetWalletBalance(10, wallet1);
    ASSERT_TRUE(balance_10block > 0);
    uint64_t balance_50block = fsns->GetWalletBalance(50, wallet1);
    ASSERT_TRUE(balance_50block > 0);
    ASSERT_TRUE(balance_10block < balance_50block);


}


TEST_F(FSNServantTest, GetBalance2)
{
    FSN_WalletData wallet1("T6T2LeLmi6hf58g7MeTA8i4rdbVY8WngXBK3oWS7pjjq9qPbcze1gvV32x7GaHx8uWHQGNFBy1JCY1qBofv56Vwb26Xr998SE",
                           "0ae7176e5332974de64713c329d406956e8ff2fd60c85e7ee6d8c88318111007");

//    std::cout << Monero::Wallet::displayAmount(fsns->GetWalletBalance(20, wallet1)) << std::endl;
//    std::cout << Monero::Wallet::displayAmount(fsns->GetWalletBalance(10, wallet1)) << std::endl;
//    for (int i = 0; i < 1; ++i) {
//        std::cout << "opening wallet: " << i << std::endl;
//        std::cout << Monero::Wallet::displayAmount(fsns->GetWalletBalance(0,  wallet1)) << std::endl;
//    }

//    ASSERT_TRUE(fsns->GetWalletBalance(0, wallet1) > 0);
//    auto start = std::chrono::high_resolution_clock::now();
//    std::this_thread::sleep_for(std::chrono::seconds(2));
//    auto end = std::chrono::high_resolution_clock::now();
//    std::chrono::duration<double, std::milli> elapsed = end-start;
//    std::cout << "Waited " << elapsed.count() << " ms\n";

}


int main(int argc, char** argv)
{
    epee::string_tools::set_module_name_and_folder(argv[0]);
    mlog_configure("", true);
    mlog_set_log_level(1);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

