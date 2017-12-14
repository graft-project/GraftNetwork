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


#include "daemon/command_server.h"
#include "daemon/daemon.h"
#include "daemon/executor.h"
#include "daemonizer/daemonizer.h"
#include "misc_log_ex.h"
#include "DAPI_RPC_Server.h"
#include "DAPI_RPC_Client.h"
#include "FSN_Servant_Test.h"
#include "PosProxy.h"
#include "WalletProxy.h"
#include "AuthSample.h"
#include "P2P_Broadcast.h"

#include <boost/bind.hpp>
#include <string>
#include <iostream>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/tokenizer.hpp>

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

*/
int main(int argc, const char** argv) {
	mlog_configure("", true);
    mlog_set_log_level(5);

// ---------------------------
//	supernode::Test_RTA_FlowBlockChain test_flow_chain;
//	test_flow_chain.Test();
//	return 0;

//	supernode::Test_RTA_Flow test_flow;
//	test_flow.Test();
//	return 0;

// ---------------------------

	string conf_file("conf.ini");
	if(argc>1) conf_file = argv[1];
	LOG_PRINT_L5("conf: "<<conf_file);

	// load config
	boost::property_tree::ptree config;
	boost::property_tree::ini_parser::read_ini(conf_file, config);

/*
	// TODO: Init all monero staff here
    // TODO:
    // 1. implement daemon as a library
    // 2. design and implement interface so supernode can talk with the daemon running in the same process
    boost::program_options::variables_map vm;
    daemonizer::daemonize(argc, argv, daemonize::t_executor{}, vm);
*/

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
