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


int main(int argc, char** argv) {
	mlog_configure("", true);
	mlog_set_log_level(5);


	TestDAPI_Server_And_Client tt1;
	tt1.Test();




	return 0;

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




