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


#include "misc_log_ex.h"
#include "DAPI_RPC_Server.h"
#include "DAPI_RPC_Client.h"
#include "FSN_Servant_Test.h"
#include "PosProxy.h"
#include "WalletProxy.h"
#include "AuthSample.h"
#include "P2P_Broadcast.h"
#include "FSN_ActualList.h"

#include "supernode_helpers.h"
#include <boost/bind.hpp>
#include <string>
#include <iostream>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/tokenizer.hpp>

using namespace std;


namespace supernode {

struct TestActualList {
	void Set(const string& p1, const string& p2, bool first, const string& basePath, const string& sw, const string& swp, const string& mw, const string& mwp) {
		string ip("127.0.0.1");
		m_DAPIServer = new DAPI_RPC_Server();
		m_DAPIServer->Set(ip, first?p1:p2, 10);
		vector<string> vv;
		vv.push_back(  ip+string(":")+p1 );
		vv.push_back(  ip+string(":")+p2 );
		m_P2P.Set(m_DAPIServer, vv);

        Servant = new FSN_Servant(basePath+"/test_blockchain", "localhost:28981", "", "", "", true);
        Servant->Set(basePath+string("/test_wallets")+sw, swp, basePath+string("/test_wallets")+mw, mwp);

		List = new FSN_ActualList(Servant, &m_P2P, m_DAPIServer);

		Tr = new boost::thread(&TestActualList::Run, this);

	}

	void Run() {

		m_DAPIServer->Start();
	}

	void Stop() {
		List->Stop();
		m_DAPIServer->Stop();
		Tr->join();
	}

    bool FindFSN(const string& port, const string& stakeW, const string& stakeKey) {
        for(unsigned i=0;i<Servant->All_FSN.size();i++) {
            auto a = Servant->All_FSN[i];
            //LOG_PRINT_L5(a->IP<<":"<<a->Port<<"  "<<a->Stake.Addr<<"  "<<a->Stake.ViewKey);
            if( a->IP=="127.0.0.1" && a->Port==port && a->Stake.Addr==stakeW && a->Stake.ViewKey==stakeKey ) return true;
        }
        return false;
    }

	DAPI_RPC_Server* m_DAPIServer = nullptr;
	P2P_Broadcast m_P2P;
	boost::thread* Tr = nullptr;
    FSN_ActualList* List = nullptr;
    FSN_Servant* Servant = nullptr;

};

/*
struct P2PTestNode {
	void Set(const string& p1, const string& p2, bool first=true) {
		string ip("127.0.0.1");
		m_DAPIServer = new DAPI_RPC_Server();
		m_DAPIServer->Set(ip, first?p1:p2, 10);
		vector<string> vv;
		vv.push_back(  ip+string(":")+p1 );
		vv.push_back(  ip+string(":")+p2 );
		m_P2P.Set(m_DAPIServer, vv);

		Tr = new boost::thread(&P2PTestNode::Run, this);

	}
	void Run() {
		m_DAPIServer->Start();
	}

	void Stop() {
		m_DAPIServer->Stop();
		Tr->join();
	}

	DAPI_RPC_Server* m_DAPIServer = nullptr;
	P2P_Broadcast m_P2P;
	boost::thread* Tr = nullptr;

};

main() {
    supernode::P2PTestNode node1;
    node1.Set("7500", "8500", true);
    supernode::P2PTestNode node2;
    node2.Set("7500", "8500", false);
    sleep(1);

    node2.m_P2P.AddHandler<supernode::rpc_command::BROADCACT_ADD_FULL_SUPER_NODE>("TestP2PHandler", bind(TestHandler, _1) );
    node1.m_P2P.AddHandler<supernode::rpc_command::BROADCACT_ADD_FULL_SUPER_NODE>("TestP2PHandler", bind(TestHandler, _1) );

    supernode::rpc_command::BROADCACT_ADD_FULL_SUPER_NODE data;
    data.IP = "192.168.45.45";
    data.Port = "0xFFDDCC";

    node1.m_P2P.Send("TestP2PHandler", data);

    sleep(1);
    node1.Stop();
    node2.Stop();

}

*/

};
/*
static void TestHandler(const supernode::rpc_command::BROADCACT_ADD_FULL_SUPER_NODE& data) {
	LOG_PRINT_L5("GOT: "<<data.IP<<":"<<data.Port);
}
*/

int main(int argc, const char** argv) {
	mlog_configure("", true);
    mlog_set_log_level(5);
/*

// ---------------------------
    LOG_PRINT_L5("START");

    string basePath = "/home/laid/Dev/Graft/GraftNetwork/tests/data/supernode";
    supernode::TestActualList node1;
    node1.Set("7500", "8500", true, basePath, "/stake_wallet", "", "/miner_wallet", "");
    supernode::TestActualList node2;
    node2.Set("7500", "8500", false, basePath, "/miner_wallet", "", "/stake_wallet", "");
    sleep(1);



    sleep(1);
    node1.Stop();
    node2.Stop();


    LOG_PRINT_L5("END");
	return 0;
*/
// ---------------------------

	string conf_file("conf.ini");
	if(argc>1) conf_file = argv[1];
	LOG_PRINT_L5("conf: "<<conf_file);

	// load config
	boost::property_tree::ptree config;
	boost::property_tree::ini_parser::read_ini(conf_file, config);


	// TODO: Init all monero staff here
    // TODO:
    // 1. implement daemon as a library
    // 2. design and implement interface so supernode can talk with the daemon running in the same process
    // boost::program_options::variables_map vm;
    // daemonizer::daemonize(argc, argv, daemonize::t_executor{}, vm);



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
	//broadcast.Set( p2p_conf.get<string>("ip"), p2p_conf.get<string>("port"), p2p_conf.get<int>("threads"), p2p_seeds );
	//broadcast.Start();


	// Init super node objects
	const boost::property_tree::ptree& dapi_conf = config.get_child("dapi");
	supernode::DAPI_RPC_Server dapi_server;
	dapi_server.Set( dapi_conf.get<string>("ip"), dapi_conf.get<string>("port"), dapi_conf.get<int>("threads") );

	// Init servant
	const boost::property_tree::ptree& cf_ser = config.get_child("servant");
	supernode::FSN_Servant_Test servant( cf_ser.get<string>("bdb_path"), cf_ser.get<string>("daemon_addr"), "", cf_ser.get<bool>("is_testnet") );
	servant.Set( cf_ser.get<string>("stake_wallet_path"), "", cf_ser.get<string>("miner_wallet_path"), "");
	// TODO: Remove next code, it only for testing
	const boost::property_tree::ptree& fsn_hardcoded = config.get_child("fsn_hardcoded");
	for(unsigned i=0;i<10000;i++) {
		string key = string("data")+boost::lexical_cast<string>(i);
		string val = fsn_hardcoded.get<string>(key, "");
		if(val=="") break;
		vector<string> vv = supernode::helpers::StrTok(val, ":");

		servant.AddFsnAccount(boost::make_shared<supernode::FSN_Data>(supernode::FSN_WalletData{vv[2], vv[3]}, supernode::FSN_WalletData{vv[4], vv[5]}, vv[0], vv[1]));
	}
	// TODO: end


	/*

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
    */


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
