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
        fsns = new FSN_Servant(db_path, "", true);

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
    const string address1 = "T6SQG5uxxtZC17hQdnapt3WjHKZnKoJS5gzz9QMvyBthEa4ChsHujswji7vdzgUos371nBjVbweVyTrqi8mxwZHh2k1KZ14WJ";
    const string viewkey1 = "582305765f0b173567926068c66b6073632b705100772ac066472d75479f2b07";
    const string address2 = "T6TyzMRMpksMftG4twjXyaC1vdoJ4axHg3xxtbWiQ5Ps3soR779vdNF2R7iEhyZ1Uicacfc8X3drQFmtzLZtnPN81TwSDmyun";
    const string viewkey2 = "455224dc3f6363fa09590efa43f5b6bdc04194d2a9c6c91e7605f7083771d20a";

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

    output = fsns->LastBlocksResolvedByFSN(20000, 10);
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
    ASSERT_TRUE(fsns->GetMyMinerWallet().Addr == "T6TyzMRMpksMftG4twjXyaC1vdoJ4axHg3xxtbWiQ5Ps3soR779vdNF2R7iEhyZ1Uicacfc8X3drQFmtzLZtnPN81TwSDmyun");
    ASSERT_TRUE(fsns->GetMyMinerWallet().ViewKey == "455224dc3f6363fa09590efa43f5b6bdc04194d2a9c6c91e7605f7083771d20a");
    ASSERT_TRUE(fsns->GetMyStakeWallet().Addr == "T6UBooqFFkN4PMmc2fH6TTDrwaJYSNHqaPchqEAWAxPU1ksQvbQynju4wn3yecsPw7gfubYYVkxbhQJmnRJLaKQu2NTJuyoRn");
    ASSERT_TRUE(fsns->GetMyStakeWallet().ViewKey == "589f8986c57cdfb4dbba870168be4faf883eccbdc838c86e32242a99e740cd01");
}

TEST_F(FSNServantTest, SignAndVerify)
{
    string wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
    fsns->Set(wallet_root_path + "/stake_wallet", "", wallet_root_path + "/miner_wallet", "");

    std::string message = "Hello, Graft";
    std::string address = "T6TyzMRMpksMftG4twjXyaC1vdoJ4axHg3xxtbWiQ5Ps3soR779vdNF2R7iEhyZ1Uicacfc8X3drQFmtzLZtnPN81TwSDmyun";

    std::string signature = fsns->SignByWalletPrivateKey(message, address);

    ASSERT_TRUE(fsns->IsSignValid(message, address, signature));
    ASSERT_FALSE(fsns->IsSignValid(message + ".", address, signature));
}



int main(int argc, char** argv)
{
    epee::string_tools::set_module_name_and_folder(argv[0]);
    mlog_configure("", true);
    mlog_set_log_level(1);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

