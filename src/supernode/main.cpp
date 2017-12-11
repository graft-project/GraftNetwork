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

#include <boost/bind.hpp>
#include <string>
#include <iostream>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/tokenizer.hpp>
#include "misc_log_ex.h"
#include "DAPI_RPC_Server.h"
#include "DAPI_RPC_Client.h"
#include "FSN_Servant.h"
#include "PosProxy.h"
#include "WalletProxy.h"
#include "AuthSample.h"
#include "P2P_Broadcast.h"
using namespace std;

namespace supernode {
namespace helpers {
vector<string> StrTok(const string& str, const string& sep) {
	vector<string> ret;
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep1(sep.c_str());
	tokenizer tokens(str, sep1);
	for(auto i=tokens.begin();i!=tokens.end();i++) ret.push_back( *i );
	return ret;
}
};
};

/*
class TestDAPI_Server_And_Client {
	public:
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

	public:
	void Test() {
		string ip = "127.0.0.1";
		string port = "7555";


		supernode::DAPI_RPC_Server dapi_server;
		dapi_server.Set( ip, port, 5 );

		boost::thread workerThread(&supernode::DAPI_RPC_Server::Start, &dapi_server);
		dapi_server.ADD_DAPI_HANDLER(MyTestCall, TestDAPI_Server_And_Client::TEST_RPC_CALL, TestDAPI_Server_And_Client);

		dapi_server.Add_UUID_MethodHandler<TEST_RPC_CALL::request, TEST_RPC_CALL::response>( "1", "Payment", bind( &TestDAPI_Server_And_Client::Pay1, this, _1, _2) );
		dapi_server.Add_UUID_MethodHandler<TEST_RPC_CALL::request, TEST_RPC_CALL::response>( "2", "Payment", bind( &TestDAPI_Server_And_Client::Pay2, this, _1, _2) );



		sleep(1);

		supernode::DAPI_RPC_Client client;
		client.Set(ip, port);

		TEST_RPC_CALL::request in;
		TEST_RPC_CALL::response out;
		in.Data = 10;
		out.Data = 0;
		bool ret = client.Invoke("MyTestCall", in, out);

		//ret && out.Data==20;

		LOG_PRINT_L5("ret: "<<ret<<"  out.D: "<<out.Data);

		in.PaymentID = "1";
		out.Data = 0;
		ret = client.Invoke("Payment", in, out);

		// ret && out.D==1
		LOG_PRINT_L5("ret: "<<ret<<"  out.D: "<<out.Data);

		in.PaymentID = "2";
		ret = client.Invoke("Payment", in, out);

		// ret && out.D==2
		LOG_PRINT_L5("ret: "<<ret<<"  out.D: "<<out.Data);


		dapi_server.Stop();

		workerThread.join();


	}



};
*/

namespace supernode {

struct Test_FSN_Servant : public supernode::FSN_ServantBase {
	public:
    vector<pair<uint64_t, boost::shared_ptr<supernode::FSN_Data>>> m_LastBlocksResolvedByFSN;
    vector<boost::shared_ptr<supernode::FSN_Data>> m_GetAuthSample;
    uint64_t m_GetCurrentBlockHeight = 0;
    string m_SignByWalletPrivateKey;
    supernode::FSN_WalletData m_GetMyStakeWallet;
    supernode::FSN_WalletData m_GetMyMinerWallet;
    unsigned m_AuthSampleSize;



	public:
    vector<pair<uint64_t, boost::shared_ptr<supernode::FSN_Data>>>
    LastBlocksResolvedByFSN(uint64_t startFromBlock, uint64_t blockNums) const override { return vector<pair<uint64_t, boost::shared_ptr<supernode::FSN_Data>>>(); }

    vector<boost::shared_ptr<supernode::FSN_Data>> GetAuthSample(uint64_t forBlockNum) const { return m_GetAuthSample; }

    uint64_t GetCurrentBlockHeight() const { return m_GetCurrentBlockHeight; }

    string SignByWalletPrivateKey(const string& str, const string& wallet_addr) const { return m_SignByWalletPrivateKey; }

    bool IsSignValid(const string& message, const string &address, const string &signature) const { return true; }

    uint64_t GetWalletBalance(uint64_t block_num, const supernode::FSN_WalletData& wallet) const { return 0; }


    supernode::FSN_WalletData GetMyStakeWallet() const { return m_GetMyStakeWallet; }
    supernode::FSN_WalletData GetMyMinerWallet() const { return m_GetMyMinerWallet; }
    unsigned AuthSampleSize() const { return m_AuthSampleSize; }

};

struct Test_RTA_Flow {
	public:
	struct Supernode {
		Test_FSN_Servant Servant;
		boost::thread* WorkerThread = nullptr;
		supernode::DAPI_RPC_Server dapi_server;

		string Port;
		string IP;

		void Run() {
			dapi_server.Set( IP, Port, 500 );


			vector<supernode::BaseRTAProcessor*> objs;
			objs.push_back( new supernode::WalletProxy() );
			objs.push_back( new supernode::PosProxy() );
			objs.push_back( new supernode::AuthSample() );

			for(unsigned i=0;i<objs.size();i++) {
				objs[i]->Set(&Servant, &dapi_server);
				objs[i]->Start();
			}

			dapi_server.Start();// block execution


			// -----------------------------------------------------------------

			for(unsigned i=0;i<objs.size();i++) {
				objs[i]->Stop();
				delete objs[i];
			}

		}


		void Start(string p1, string p2, string mp) {
			IP = "127.0.0.1";
			Port = mp;

			Servant.m_AuthSampleSize = 2;
			Servant.m_GetCurrentBlockHeight = 13;

			boost::shared_ptr<FSN_Data> d1 = boost::shared_ptr<FSN_Data>(new FSN_Data());
			boost::shared_ptr<FSN_Data> d2 = boost::shared_ptr<FSN_Data>(new FSN_Data());
			d1->IP = IP;
			d1->Port = p1;
			d1->Stake.Addr = "1_fsn";
			d2->IP = IP;
			d2->Port = p2;
			d2->Stake.Addr = "2_fsn";

			Servant.m_GetAuthSample.push_back(d1);
			Servant.m_GetAuthSample.push_back(d2);


			WorkerThread = new boost::thread(&Supernode::Run, this);
		}

		void Stop() {
			dapi_server.Stop();
			WorkerThread->join();
		}



	};



	NTransactionStatus GetPayStatus(const string& payID) {
		DAPI_RPC_Client call;
		call.Set(IP, WalletProxyPort);

		rpc_command::WALLET_GET_TRANSACTION_STATUS::request in;
		rpc_command::WALLET_GET_TRANSACTION_STATUS::response out;
		in.PaymentID = payID;
		bool ret = call.Invoke(dapi_call::GetPayStatus, in, out, chrono::seconds(10));

        if(!ret) return NTransactionStatus::Fail;
        return NTransactionStatus(out.Status);
	}

	NTransactionStatus GetSaleStatus(const string& payID) {
		DAPI_RPC_Client call;
		call.Set(IP, PosProxyPort);

		rpc_command::POS_GET_SALE_STATUS::request in;
		rpc_command::POS_GET_SALE_STATUS::response out;
		in.PaymentID = payID;
		bool ret = call.Invoke(dapi_call::GetSaleStatus, in, out, chrono::seconds(10));

        if(!ret) return NTransactionStatus::Fail;
        return NTransactionStatus(out.Status);
	}

	string IP = "127.0.0.1";;
	string WalletProxyPort = "7500";;
	string PosProxyPort = "8500";;
	bool Verbose = true;

	bool Assert(bool bb, const string& str) {
		if(!Verbose) return bb;
		LOG_PRINT_L5(str<<" - "<<(bb?"OK":"Fail"));
		return bb;
	}

	bool DoTest() {
		bool bb;

		rpc_command::POS_SALE::request sale_in;
		rpc_command::POS_SALE::response sale_out;
        sale_in.Amount = 11;
        sale_in.POSSaleDetails = "Some data";
        sale_in.POSAddress = "0xFF";

		unsigned repeatCount = 10;


		for(unsigned i=0;i<repeatCount;i++) {// transaction must started from Sale call
			DAPI_RPC_Client pos_sale;
			pos_sale.Set(IP, PosProxyPort);
			bb = pos_sale.Invoke("Sale", sale_in, sale_out, chrono::seconds(10));
			if( Assert(bb, "Sale") ) break;

			//LOG_PRINT_L5("Sale ret: "<<ret<<"  BlockNum: "<<sale_out.BlockNum<<"  uuid: "<<sale_out.PaymentID);
		}
		if(!bb) return false;

		for(unsigned i=0;i<repeatCount;i++) {// after sale call you get PaymentID and BlockNum and can start poll status by GetSaleStatus call
			NTransactionStatus trs =  GetSaleStatus(sale_out.PaymentID);
			bb = trs==NTransactionStatus::InProgress;
			if( Assert(bb, "GetSaleStatus") ) break;
			//boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
			//LOG_PRINT_L5("GetSaleStatus: "<<()<<"  int: "<<int(trs));
		}
		if(!bb) return false;

		for(unsigned i=0;i<repeatCount;i++) {// in any time after Sale call you can get PoS data by WalletGetPosData call
			rpc_command::WALLET_GET_POS_DATA::request in;
			rpc_command::WALLET_GET_POS_DATA::response out;
			in.BlockNum = sale_out.BlockNum;
			in.PaymentID = sale_out.PaymentID;
			DAPI_RPC_Client call;
			call.Set(IP, WalletProxyPort);
			bb = call.Invoke("WalletGetPosData", in, out, chrono::seconds(10));
            bb = bb && out.POSSaleDetails==sale_in.POSSaleDetails;
			if( Assert(bb, "WalletGetPosData") ) break;
			//boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

			//LOG_PRINT_L5("WalletGetPosData ret: "<<ret<<"  data: "<<out.DataForClientWallet);
		}
		if(!bb) return false;


		// after use push Pay button, send Pay call
		rpc_command::WALLET_PAY::request pay_in;
		rpc_command::WALLET_PAY::response pay_out;
        pay_in.Amount = sale_in.Amount;
        pay_in.POSAddress = sale_in.POSAddress;
		pay_in.BlockNum = sale_out.BlockNum;
		pay_in.PaymentID = sale_out.PaymentID;
		for(unsigned i=0;i<repeatCount;i++) {
			DAPI_RPC_Client wallet_pay;
			wallet_pay.Set(IP, WalletProxyPort);
			bb = wallet_pay.Invoke("Pay", pay_in, pay_out, chrono::seconds(10));
			if( Assert(bb, "Pay") ) break;
			//boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
			//LOG_PRINT_L5("Pay ret: "<<ret);
		}
		if(!bb) return false;

		for(unsigned i=0;i<repeatCount;i++) {// after Pay call you can can start poll status by GetPayStatus call
			NTransactionStatus trs =  GetPayStatus(sale_out.PaymentID);
			bb = trs==NTransactionStatus::Success;
			if( Assert(bb, "GetPayStatus") ) break;
			//boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
			//LOG_PRINT_L5("GetPayStatus: "<<(trs==NTRansactionStatus::Success)<<"  int: "<<int(trs));

		}
		if(!bb) return false;


		for(unsigned i=0;i<repeatCount;i++) {
			NTransactionStatus trs =  GetSaleStatus(sale_out.PaymentID);
			bb = trs==NTransactionStatus::Success;
			if( Assert(bb, "GetSaleStatus") ) break;
			//boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
			//LOG_PRINT_L5("GetSaleStatus2: "<<(trs==NTRansactionStatus::Success)<<"  int: "<<int(trs));
		}
		if(!bb) return false;


		return true;
	}

	void TestWalletReject() {
		bool bb;

		rpc_command::POS_SALE::request sale_in;
		rpc_command::POS_SALE::response sale_out;
        sale_in.Amount = 11;
        sale_in.POSSaleDetails = "Some data";
        sale_in.POSAddress = "0xFF";

		unsigned repeatCount = 10;


		for(unsigned i=0;i<repeatCount;i++) {// transaction must started from Sale call
			DAPI_RPC_Client pos_sale;
			pos_sale.Set(IP, PosProxyPort);
			bb = pos_sale.Invoke("Sale", sale_in, sale_out, chrono::seconds(10));
			if( Assert(bb, "Sale") ) break;
		}
		if(!bb) return;

		for(unsigned i=0;i<repeatCount;i++) {// after sale call you get PaymentID and BlockNum and can start poll status by GetSaleStatus call
			NTransactionStatus trs =  GetSaleStatus(sale_out.PaymentID);
			bb = trs==NTransactionStatus::InProgress;
			if( Assert(bb, "GetSaleStatus") ) break;
		}
		if(!bb) return;

		for(unsigned i=0;i<repeatCount;i++) {// in any time after Sale call you can get PoS data by WalletGetPosData call
			rpc_command::WALLET_REJECT_PAY::request in;
			rpc_command::WALLET_REJECT_PAY::response out;
			in.BlockNum = sale_out.BlockNum;
			in.PaymentID = sale_out.PaymentID;
			DAPI_RPC_Client call;
			call.Set(IP, WalletProxyPort);
			bb = call.Invoke("WalletRejectPay", in, out, chrono::seconds(10));
			if( Assert(bb, "WalletRejectPay") ) break;
		}
		if(!bb) return;



		for(unsigned i=0;i<repeatCount;i++) {// after Pay call you can can start poll status by GetPayStatus call
			NTransactionStatus trs =  GetSaleStatus(sale_out.PaymentID);
			bb = trs==NTransactionStatus::RejectedByWallet;
			if( Assert(bb, "GetSaleStatus : RejectedByWallet") ) break;
		}
		if(!bb) return;

		LOG_PRINT_L5("WalletRejectPay - OK ");

	}

	unsigned m_RunInTread = 10;
	atomic_uint m_Fail = {0};
	void TestThread() {
		for(unsigned i=0;i<100;i++) {
			LOG_PRINT_L5("\n");
			if( !DoTest() ) { m_Fail++; }
			//boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
		}
	}

	void Test() {
		Supernode wallet_proxy;
		wallet_proxy.Start(WalletProxyPort, PosProxyPort, WalletProxyPort);

		Supernode pos_proxy;
		pos_proxy.Start(WalletProxyPort, PosProxyPort, PosProxyPort);

		sleep(1);

		TestWalletReject();




		boost::thread_group workers;
		for(int i=0;i<10;i++) {
			workers.create_thread( boost::bind(&Test_RTA_Flow::TestThread, this) );
		}
		workers.join_all();
		LOG_PRINT_L5("\n\nFAILED count: "<<m_Fail);


		wallet_proxy.Stop();
		pos_proxy.Stop();
	}

};

};

int main(int argc, char** argv) {
	mlog_configure("", true);
    mlog_set_log_level(5);

//	supernode::Test_RTA_Flow test_flow;
//	test_flow.Test();
//	return 0;


/*
	TestDAPI_Server_And_Client tt1;
	tt1.Test();
	return 0;
*/

	string conf_file("conf.ini");
	if(argc>1) conf_file = argv[1];
	LOG_PRINT_L5("conf: "<<conf_file);

	// load config
	boost::property_tree::ptree config;
	boost::property_tree::ini_parser::read_ini(conf_file, config);


	// TODO: Init all monero staff here

	// init p2p
	const boost::property_tree::ptree& p2p_conf = config.get_child("p2p");
	vector< pair<string, string> > p2p_seeds;

    {// for test only
    	vector<string> ipps = supernode::helpers::StrTok( p2p_conf.get<string>("seeds"), "," );
    	for(auto ipp : ipps ) {
    		vector<string> vv = supernode::helpers::StrTok( ipp, ":" );
    		p2p_seeds.push_back( make_pair(vv[0], vv[1]) );
    	}
    }


	supernode::P2P_Broadcast broadcast;
	broadcast.Set( p2p_conf.get<string>("ip"), p2p_conf.get<string>("port"), p2p_conf.get<int>("threads"), p2p_seeds );
	broadcast.Start();


	// Init super node objects
	const boost::property_tree::ptree& dapi_conf = config.get_child("dapi");
	supernode::DAPI_RPC_Server dapi_server;
	dapi_server.Set( dapi_conf.get<string>("ip"), dapi_conf.get<string>("port"), dapi_conf.get<int>("threads") );

    const boost::property_tree::ptree& wc = config.get_child("wallets");
//    supernode::FSN_Servant servant;
//    servant.Set( wc.get<string>("stake_file"), wc.get<string>("stake_passwd"), wc.get<string>("miner_file"), wc.get<string>("miner_passwd") );
    //TODO: Remove next code, it only for testing
    supernode::Test_FSN_Servant servant;
    servant.m_AuthSampleSize = 1;
    boost::shared_ptr<supernode::FSN_Data> d1 = boost::shared_ptr<supernode::FSN_Data>(new supernode::FSN_Data());
    d1->IP = dapi_conf.get<string>("ip");
    d1->Port = dapi_conf.get<string>("port");
    d1->Stake.Addr = "1_fsn";
    servant.m_GetAuthSample.push_back(d1);
    //TODO: end

	vector<supernode::BaseRTAProcessor*> objs;
	objs.push_back( new supernode::WalletProxy() );
	objs.push_back( new supernode::PosProxy() );
	objs.push_back( new supernode::AuthSample() );

	for(unsigned i=0;i<objs.size();i++) {
		objs[i]->Set(&servant, &dapi_server);
		objs[i]->Start();
	}

	dapi_server.Start();// block execution


	// -----------------------------------------------------------------



	broadcast.Stop();// because handlers not deleted, so stop first, then delete objects

	for(unsigned i=0;i<objs.size();i++) {
		objs[i]->Stop();
		delete objs[i];
	}

    return 0;
}
