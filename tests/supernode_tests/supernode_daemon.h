#ifndef SUPERNODE_DAEMON_H
#define SUPERNODE_DAEMON_H

#include <boost/property_tree/ini_parser.hpp>
#include "supernode/supernode_helpers.h"
#include "supernode/FSN_Servant.h"
#include "supernode/FSN_Servant_Test.h"
#include "supernode/DAPI_RPC_Server.h"
#include "supernode/WalletProxy.h"
#include "supernode/AuthSample.h"
#include "supernode/PosProxy.h"

using namespace supernode;

struct Supernode
{
    FSN_Servant* Servant = nullptr;
    boost::thread* WorkerThread = nullptr;
    supernode::DAPI_RPC_Server dapi_server;
    string s_TestDataPath;

    atomic_uint Started = {0};


    Supernode(const string& path = std::string()) { s_TestDataPath = path; }

    void Run()
    {
        vector<supernode::BaseRTAProcessor*> objs;
        objs.push_back(new supernode::WalletProxy());
        if (!supernode::rpc_command::IsWalletProxyOnly())
        {
            objs.push_back(new supernode::PosProxy());
            objs.push_back(new supernode::AuthSample());
        }
        for (unsigned i=0;i<objs.size();i++)
        {
            objs[i]->Set(Servant, &dapi_server);
            objs[i]->Start();
        }

        Started = 1;
        dapi_server.Start();// block execution


        // -----------------------------------------------------------------

        for (unsigned i=0;i<objs.size();i++)
        {
            objs[i]->Stop();
            delete objs[i];
        }
    }


    void Start(string conf_file)
    {
        boost::property_tree::ptree config;
        boost::property_tree::ini_parser::read_ini(conf_file, config);

        const boost::property_tree::ptree& dapi_conf = config.get_child("dapi");
        supernode::rpc_command::SetDAPIVersion(dapi_conf.get<string>("version"));
        dapi_server.Set(dapi_conf.get<string>("ip"), dapi_conf.get<string>("port"),
                        dapi_conf.get<int>("threads"));
        supernode::rpc_command::SetWalletProxyOnly( dapi_conf.get<int>("wallet_proxy_only", 0)==1);

        const boost::property_tree::ptree& cf_ser = config.get_child("servant");
        Servant = new FSN_Servant_Test(cf_ser.get<string>("bdb_path"),
                                       cf_ser.get<string>("daemon_addr"), "",
                                       cf_ser.get<bool>("is_testnet"));
        if(!supernode::rpc_command::IsWalletProxyOnly())
        {
            Servant->Set(cf_ser.get<string>("stake_wallet_path"), "",
                         cf_ser.get<string>("miner_wallet_path"), "");
            const boost::property_tree::ptree& fsn_hardcoded = config.get_child("fsn_hardcoded");
            for(unsigned i=1;i<10000;i++)
            {
                string key = string("data")+boost::lexical_cast<string>(i);
                string val = fsn_hardcoded.get<string>(key, "");
                if(val=="") break;
                vector<string> vv = supernode::helpers::StrTok(val, ":");

                Servant->AddFsnAccount(boost::make_shared<supernode::FSN_Data>(supernode::FSN_WalletData{vv[2], vv[3]}, supernode::FSN_WalletData{vv[4], vv[5]}, vv[0], vv[1]));
            }
        }

        WorkerThread = new boost::thread(&Supernode::Run, this);
    }

    void Stop()
    {
        dapi_server.Stop();
        WorkerThread->join();
    }
};

#endif /* SUPERNODE_DAEMON_H */
