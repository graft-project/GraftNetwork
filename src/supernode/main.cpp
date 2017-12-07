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
			dapi_server.Set( IP, Port, 5 );


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


	NTRansactionStatus GetPayStatus(const string& port, const string& payID) {
		DAPI_RPC_Client call;
		call.Set("127.0.0.1", port);

		rpc_command::WALLET_GET_TRANSACTION_STATUS::request in;
		rpc_command::WALLET_GET_TRANSACTION_STATUS::response out;
		in.PaymentID = payID;
		bool ret = call.Invoke(dapi_call::GetPayStatus, in, out);

		if(!ret) return NTRansactionStatus::Fail;
		return NTRansactionStatus(out.Status);
	}

	NTRansactionStatus GetSaleStatus(const string& port, const string& payID) {
		DAPI_RPC_Client call;
		call.Set("127.0.0.1", port);

		rpc_command::POS_GET_SALE_STATUS::request in;
		rpc_command::POS_GET_SALE_STATUS::response out;
		in.PaymentID = payID;
		bool ret = call.Invoke(dapi_call::GetSaleStatus, in, out);

		if(!ret) return NTRansactionStatus::Fail;
		return NTRansactionStatus(out.Status);
	}

	void Test() {
		string ip = "127.0.0.1";
		string p1 = "7500";
		string p2 = "8500";

		Supernode wallet_proxy;
		wallet_proxy.Start(p1, p2, p1);

		Supernode pos_proxy;
		pos_proxy.Start(p1, p2, p2);

		sleep(1);

		rpc_command::POS_SALE::request sale_in;
		rpc_command::POS_SALE::response sale_out;
		sale_in.Sum = 11;
		sale_in.DataForClientWallet = "Some data";
		sale_in.POS_Wallet = "0xFF";

		{// transaction must started from Sale call
			DAPI_RPC_Client pos_sale;
			pos_sale.Set(ip, p2);
			bool ret = pos_sale.Invoke("Sale", sale_in, sale_out);
			LOG_PRINT_L5("Sale ret: "<<ret<<"  BlockNum: "<<sale_out.BlockNum<<"  uuid: "<<sale_out.PaymentID);
		}

		{// after sale call you get PaymentID and BlockNum and can start poll status by GetSaleStatus call
			NTRansactionStatus trs =  GetSaleStatus(p2, sale_out.PaymentID);
			LOG_PRINT_L5("GetSaleStatus: "<<(trs==NTRansactionStatus::InProgress)<<"  int: "<<int(trs));

		}

		{// in any time after Sale call you can get PoS data by WalletGetPosData call
			rpc_command::WALLET_GET_POS_DATA::request in;
			rpc_command::WALLET_GET_POS_DATA::response out;
			in.BlockNum = sale_out.BlockNum;
			in.PaymentID = sale_out.PaymentID;
			DAPI_RPC_Client call;
			call.Set(ip, p1);
			bool ret = call.Invoke("WalletGetPosData", in, out);

			LOG_PRINT_L5("WalletGetPosData ret: "<<ret<<"  data: "<<out.DataForClientWallet);
		}


		// after use push Pay button, send Pay call
		rpc_command::WALLET_PAY::request pay_in;
		rpc_command::WALLET_PAY::response pay_out;
		pay_in.Sum = sale_in.Sum;
		pay_in.POS_Wallet = sale_in.POS_Wallet;
		pay_in.BlockNum = sale_out.BlockNum;
		pay_in.PaymentID = sale_out.PaymentID;
		{
			DAPI_RPC_Client wallet_pay;
			wallet_pay.Set(ip, p1);
			bool ret = wallet_pay.Invoke("Pay", pay_in, pay_out);

			LOG_PRINT_L5("Pay ret: "<<ret<<"  data: "<<pay_out.DataForClientWallet);
		}

		{// after Pay call you can can start poll status by GetPayStatus call
			NTRansactionStatus trs =  GetPayStatus(p1, sale_out.PaymentID);
			LOG_PRINT_L5("GetPayStatus: "<<(trs==NTRansactionStatus::Success)<<"  int: "<<int(trs));

		}

		{
			NTRansactionStatus trs =  GetSaleStatus(p2, sale_out.PaymentID);
			LOG_PRINT_L5("GetSaleStatus2: "<<(trs==NTRansactionStatus::Success)<<"  int: "<<int(trs));

		}


		wallet_proxy.Stop();
		pos_proxy.Stop();
	}

};

};

int main(int argc, char** argv) {
	mlog_configure("", true);
	mlog_set_log_level(5);

	supernode::Test_RTA_Flow test_flow;
	test_flow.Test();
	return 0;


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
	supernode::FSN_Servant servant;
	servant.Set( wc.get<string>("stake_file"), wc.get<string>("stake_passwd"), wc.get<string>("miner_file"), wc.get<string>("miner_passwd") );

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
