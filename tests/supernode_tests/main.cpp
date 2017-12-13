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
#include "supernode/DAPI_RPC_Server.h"
#include "supernode/DAPI_RPC_Client.h"
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


struct TestDAPI_Server_And_ClientBase : public testing::Test {
	struct TEST_RPC_CALL {
		struct request {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(Data)
				KV_SERIALIZE(PaymentID)
			END_KV_SERIALIZE_MAP()

			int Data;
			string PaymentID;
		};
		struct response {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(Data)
				KV_SERIALIZE(PaymentID)
			END_KV_SERIALIZE_MAP()

			int Data;
			string PaymentID;
		};
	};

	bool MyTestCall(const TEST_RPC_CALL::request& req, TEST_RPC_CALL::response& out) {
		out.Data = req.Data*2;
		return true;
	}

	bool Pay1(const TEST_RPC_CALL::request& req, TEST_RPC_CALL::response& out) {
		if(req.PaymentID!="1") return false;
		out.Data = 1;
		return true;
	}

	bool Pay2(const TEST_RPC_CALL::request& req, TEST_RPC_CALL::response& out) {
		if(req.PaymentID!="2") return false;
		out.Data = 2;
		return true;
	}

};

TEST_F(TestDAPI_Server_And_ClientBase, TestDAPI_Server_And_Client) {
		string ip = "127.0.0.1";
		string port = "7555";


		supernode::DAPI_RPC_Server dapi_server;
		dapi_server.Set( ip, port, 5 );

		boost::thread workerThread(&supernode::DAPI_RPC_Server::Start, &dapi_server);
		dapi_server.ADD_DAPI_HANDLER(MyTestCall, TestDAPI_Server_And_ClientBase::TEST_RPC_CALL, TestDAPI_Server_And_ClientBase);

		dapi_server.Add_UUID_MethodHandler<TEST_RPC_CALL::request, TEST_RPC_CALL::response>( "1", "Payment", bind( &TestDAPI_Server_And_ClientBase::Pay1, this, _1, _2) );
		dapi_server.Add_UUID_MethodHandler<TEST_RPC_CALL::request, TEST_RPC_CALL::response>( "2", "Payment", bind( &TestDAPI_Server_And_ClientBase::Pay2, this, _1, _2) );



		sleep(1);

		supernode::DAPI_RPC_Client client;
		client.Set(ip, port);

		TEST_RPC_CALL::request in;
		TEST_RPC_CALL::response out;
		in.Data = 10;
		out.Data = 0;
		bool ret = client.Invoke("MyTestCall", in, out);

		ASSERT_TRUE(ret && out.Data==20);

//		LOG_PRINT_L5("ret: "<<ret<<"  out.D: "<<out.Data);

		in.PaymentID = "1";
		out.Data = 0;
		ret = client.Invoke("Payment", in, out);

		ASSERT_TRUE(ret && out.Data==1);
//		LOG_PRINT_L5("ret: "<<ret<<"  out.D: "<<out.Data);

		in.PaymentID = "2";
		ret = client.Invoke("Payment", in, out);

		ASSERT_TRUE(ret && out.Data==2);
//		LOG_PRINT_L5("ret: "<<ret<<"  out.D: "<<out.Data);


		dapi_server.Stop();

		workerThread.join();

};



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


TEST_F(FSNServantTest, GetBalance1)
{
    FSN_WalletData wallet1("T6TyzMRMpksMftG4twjXyaC1vdoJ4axHg3xxtbWiQ5Ps3soR779vdNF2R7iEhyZ1Uicacfc8X3drQFmtzLZtnPN81TwSDmyun",
                           "455224dc3f6363fa09590efa43f5b6bdc04194d2a9c6c91e7605f7083771d20a");

    uint64_t balance_10block = fsns->GetWalletBalance(10, wallet1);
    ASSERT_TRUE(balance_10block > 0);
    uint64_t balance_50block = fsns->GetWalletBalance(50, wallet1);
    ASSERT_TRUE(balance_50block > 0);
    ASSERT_TRUE(balance_10block < balance_50block);


}


TEST_F(FSNServantTest, GetBalance2)
{
    FSN_WalletData wallet1("T6TyzMRMpksMftG4twjXyaC1vdoJ4axHg3xxtbWiQ5Ps3soR779vdNF2R7iEhyZ1Uicacfc8X3drQFmtzLZtnPN81TwSDmyun",
                           "455224dc3f6363fa09590efa43f5b6bdc04194d2a9c6c91e7605f7083771d20a");

//    std::cout << Monero::Wallet::displayAmount(fsns->GetWalletBalance(20, wallet1)) << std::endl;
//    std::cout << Monero::Wallet::displayAmount(fsns->GetWalletBalance(10, wallet1)) << std::endl;
//    for (int i = 0; i < 1; ++i) {
//        std::cout << "opening wallet: " << i << std::endl;
//        std::cout << Monero::Wallet::displayAmount(fsns->GetWalletBalance(0,  wallet1)) << std::endl;
//    }

    ASSERT_TRUE(fsns->GetWalletBalance(0, wallet1) > 0);
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

