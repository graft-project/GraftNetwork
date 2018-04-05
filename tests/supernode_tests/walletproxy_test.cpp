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
//#include "wallet/wallet2.h"

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

#include "supernode/FSN_Servant.h"
#include "supernode/FSN_Servant_Test.h"
#include "supernode/TxPool.h"
#include "supernode/WalletProxy.h"
#include "supernode/WalletPayObject.h"
#include "supernode/graft_wallet.h"
#include "supernode/PosProxy.h"
#include "supernode/AuthSample.h"
#include "supernode/api/pending_transaction.h"

#include "wallet/wallet2_api.h"


using namespace supernode;
using namespace tools;
using namespace Monero;


struct Supernode
{
    FSN_Servant* Servant = nullptr;
    boost::thread* WorkerThread = nullptr;
    supernode::DAPI_RPC_Server dapi_server;
    string s_TestDataPath;

    string Port;
    string IP;
    atomic_uint Started = {0};


    Supernode(const string& path) { s_TestDataPath = path; }

    void Run()
    {
        dapi_server.Set( IP, Port, 500 );


        vector<supernode::BaseRTAProcessor*> objs;
        objs.push_back( new supernode::WalletProxy() );
        objs.push_back( new supernode::PosProxy() );
        objs.push_back( new supernode::AuthSample() );

        for(unsigned i=0;i<objs.size();i++) {
            objs[i]->Set(Servant, &dapi_server);
            objs[i]->Start();
        }

        LOG_PRINT_L0("DAPI START on: "<<Port);
        Started = 1;
        dapi_server.Start();// block execution


        // -----------------------------------------------------------------

        for(unsigned i=0;i<objs.size();i++) {
            objs[i]->Stop();
            delete objs[i];
        }

    }


    void Start(string p1, string p2, bool second)
    {
        IP = "127.0.0.1";
        Port = second?p2:p1;


        string db_path = s_TestDataPath + "/test_blockchain";
        string wallet_root_path = s_TestDataPath + "/test_wallets";

        string wss1 = "/stake_wallet";
        string wss2 = "/miner_wallet";
        if(second) swap(wss1, wss2);


        Servant = new FSN_Servant_Test(db_path, "localhost:28281", "", true);
        Servant->Set(wallet_root_path + wss1, "", wallet_root_path + wss2, "");

        // wallet1
        string address1 = "T6T2LeLmi6hf58g7MeTA8i4rdbVY8WngXBK3oWS7pjjq9qPbcze1gvV32x7GaHx8uWHQGNFBy1JCY1qBofv56Vwb26Xr998SE";
        string viewkey1 = "0ae7176e5332974de64713c329d406956e8ff2fd60c85e7ee6d8c88318111007";
        // wallet2
        string address2 = "T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa";
        string viewkey2 = "8c0ccff03e9f2a9805e200f887731129495ff793dc678db6c5b53df814084f04";


        Servant->AddFsnAccount(boost::make_shared<FSN_Data>(FSN_WalletData{address1, viewkey1}, FSN_WalletData{address2, viewkey2}, "127.0.0.1", p1));
        Servant->AddFsnAccount(boost::make_shared<FSN_Data>(FSN_WalletData{address2, viewkey2}, FSN_WalletData{address1, viewkey1}, "127.0.0.1", p2));


        LOG_PRINT_L0("STARTED: "<<(second?2:1));


        WorkerThread = new boost::thread(&Supernode::Run, this);
    }

    void Stop()
    {
        dapi_server.Stop();
        WorkerThread->join();
    }



};


struct GraftRTATest1 : public testing::Test
{
    std::string wallet_account;
    std::string wallet_root_path;
    std::string bdb_path;
    const std::string DAEMON_ADDR = "localhost:28281";
    const uint64_t AMOUNT_10_GRF = 10000000000000;
    const std::string DST_WALLET_ADDR = "T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa";
    const std::string DST_WALLET_VIEWKEY = "8c0ccff03e9f2a9805e200f887731129495ff793dc678db6c5b53df814084f04";


    string IP = "127.0.0.1";
    string WalletProxyPort = "7500";
    string PosProxyPort = "8500";

    GraftRTATest1()
    {
        GraftWallet *wallet = new GraftWallet(true, false);
        wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
        string wallet_path = wallet_root_path + "/miner_wallet";
        bdb_path  = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_blockchain";
        wallet->load(wallet_path, "");
        // serialize test wallet
        wallet_account = wallet->store_keys_graft("", false);
        delete wallet;
    }

    ~GraftRTATest1()
    {

    }

    NTransactionStatus GetPayStatus(const string& payID) {
        DAPI_RPC_Client call;
        call.Set(IP, WalletProxyPort);

        rpc_command::WALLET_GET_TRANSACTION_STATUS::request in;
        rpc_command::WALLET_GET_TRANSACTION_STATUS::response out;
        in.PaymentID = payID;
        bool ret = call.Invoke(dapi_call::GetPayStatus, in, out);

        if(!ret) return NTransactionStatus::Fail;
        return NTransactionStatus(out.Status);
    }

    NTransactionStatus GetSaleStatus(const string& payID) {
        DAPI_RPC_Client call;
        call.Set(IP, PosProxyPort);

        rpc_command::POS_GET_SALE_STATUS::request in;
        rpc_command::POS_GET_SALE_STATUS::response out;
        in.PaymentID = payID;
        bool ret = call.Invoke(dapi_call::GetSaleStatus, in, out);

        if(!ret) return NTransactionStatus::Fail;
        return NTransactionStatus(out.Status);
    }


    bool Verbose = true;

    bool Assert(bool bb, const string& str) {
        if(!Verbose) return bb;
        LOG_PRINT_L0(str<<" - "<<(bb?"OK":"Fail"));
        return bb;
    }


    bool testWalletProxyPay()
    {
        bool result;

        rpc_command::POS_SALE::request sale_in;
        rpc_command::POS_SALE::response sale_out;

        sale_in.Amount = AMOUNT_10_GRF;
        sale_in.POSSaleDetails = "Some data";
        // 1. what is POS address here?
        sale_in.POSAddress = DST_WALLET_ADDR;
        sale_in.POSViewKey = DST_WALLET_VIEWKEY;

        unsigned repeatCount = 1;

        for (unsigned i=0; i < repeatCount; i++) {// transaction must started from Sale call
            DAPI_RPC_Client pos_sale;
            pos_sale.Set(IP, PosProxyPort);
            result = pos_sale.Invoke("Sale", sale_in, sale_out);
            if (Assert(result, "Sale"))
                break;
        }

        if (!result)
            return false;

        for (unsigned i = 0; i < repeatCount; i++) {// after sale call you get PaymentID and BlockNum and can start poll status by GetSaleStatus call
            NTransactionStatus trs =  GetSaleStatus(sale_out.PaymentID);
            result = trs == NTransactionStatus::InProgress;
            if (Assert(result, "GetSaleStatus"))
                break;
        }
        if (!result)
            return false;

        // after use push Pay button, send Pay call
        rpc_command::WALLET_PAY::request pay_in;
        rpc_command::WALLET_PAY::response pay_out;

        pay_in.Amount = sale_in.Amount;
        pay_in.POSAddress = sale_in.POSAddress;
        pay_in.BlockNum = sale_out.BlockNum;
        pay_in.PaymentID = sale_out.PaymentID;
        pay_in.Account = wallet_account;

        // 2. where do we get .Account field here ?

        for(unsigned i=0;i<repeatCount;i++) {
            DAPI_RPC_Client wallet_pay;
            wallet_pay.Set(IP, WalletProxyPort);
            LOG_PRINT_L0("Invoking 'Pay'");
            result = wallet_pay.Invoke("Pay", pay_in, pay_out);
            if (Assert(result, "Pay"))
                break;
            //boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            //LOG_PRINT_L0("Pay ret: "<<ret);
        }
        if (!result)
            return false;

        for (unsigned i=0; i < repeatCount; i++) {// after Pay call you can can start poll status by GetPayStatus call
            NTransactionStatus trs = GetPayStatus(sale_out.PaymentID);
            result = trs==NTransactionStatus::Success;
            if (Assert(result, "GetPayStatus"))
                break;
            //boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
            //LOG_PRINT_L0("GetPayStatus: "<<(trs==NTRansactionStatus::Success)<<"  int: "<<int(trs));

        }
        if (!result)
            return false;


        for (unsigned i=0; i <repeatCount; i++) {// after Pay call you can can start poll status by GetPayStatus call
            NTransactionStatus trs =  GetSaleStatus(sale_out.PaymentID);
            result = trs==NTransactionStatus::Success;
            if( Assert(result, "GetSaleStatus : Success") ) break;
        }
        return result;

    }

};

/*
TEST_F(WalletProxyTest, SendTx)
{
    ASSERT_FALSE(wallet_account.empty());


    FSN_Servant * servant = new FSN_Servant(bdb_path, DAEMON_ADDR, "", "", "/tmp/graft/fsn-wallets", true);
    DAPI_RPC_Server * dapi = new DAPI_RPC_Server();
    WalletProxy * walletProxy = new WalletProxy();

    ASSERT_EQ(servant->IsTestnet(), true);
    ASSERT_EQ(servant->GetNodeAddress(), DAEMON_ADDR);
    ASSERT_TRUE(servant->GetNodeIp() == "localhost");
    ASSERT_TRUE(servant->GetNodePort() == 28281);


    walletProxy->Set(servant, dapi);


    std::unique_ptr<GraftWallet> wallet = walletProxy->initWallet(wallet_account, "");
    ASSERT_TRUE(wallet.get());
    wallet->refresh();

    GraftTxExtra tx_extra;
    tx_extra.BlockNum = 123;
    tx_extra.PaymentID = "Hello";
    tx_extra.Signs.push_back("T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa");
    tx_extra.Signs.push_back("T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa");
    tx_extra.Signs.push_back("T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa");

    PendingTransaction * ptx =
            wallet->createTransaction(DST_WALLET_ADDR,
                                      "",
                                      AMOUNT_10_GRF,
                                      0,
                                      tx_extra,
                                      Monero::PendingTransaction::Priority_Medium
                                      );


    ASSERT_TRUE(ptx != nullptr);
    std::cout << "tx status: " << ptx->status() << std::endl;
    std::cout << "tx status: " << ptx->errorString() << std::endl;

    ASSERT_TRUE(ptx->status() == Monero::PendingTransaction::Status_Ok);
    std::cout << "sending : " << Wallet::displayAmount(ptx->amount()) << std::endl;
    bool commit_result = ptx->commit();
    wallet->refresh();
    std::cout << "ptx error: " << ptx->errorString() << std::endl;
    std::cout << "commit  status: " << commit_result << std::endl;
    ASSERT_TRUE(ptx->txid().size() > 0);
    std::cout << "sent tx: " << ptx->txid()[0] << std::endl;
    TxPool * txPool = new TxPool(DAEMON_ADDR, "", "");
    ASSERT_TRUE(ptx->txid().size() > 0);
    cryptonote::transaction tx;
    std::string tx_id = ptx->txid()[0];

    ASSERT_TRUE(txPool->get(tx_id, tx));

    crypto::hash hash;
    epee::string_tools::hex_to_pod(tx_id, hash);

    ASSERT_EQ(hash, tx.hash);

    GraftTxExtra out_extra;
    ASSERT_TRUE(cryptonote::get_graft_tx_extra_from_extra(tx, out_extra));
    delete ptx;

    ASSERT_EQ(tx_extra, out_extra);



    //    rpc_command::WALLET_PAY::request req;
    //    rpc_command::WALLET_PAY::response resp;
    //    req.Account = wallet_account;
    //    req.Amount = 1000000000; // 1 GRF
    //    ASSERT_TRUE(walletProxy->Pay(req, resp));

}
*/

TEST_F(GraftRTATest1, TestWalletProxyPay)
{
    //    ASSERT_FALSE(wallet_account.empty());
    //    const std::string DST_WALLET_ADDR = "T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa";


    //    FSN_Servant * servant = new FSN_Servant_Test(bdb_path, DAEMON_ADDR, "/tmp/graft/fsn-wallets", true);
    //    DAPI_RPC_Server * dapi = new DAPI_RPC_Server();
    //    WalletProxy * walletProxy = new WalletProxy();


    //    // should we get daemon address / login from servant?
    //    walletProxy->Set(servant, dapi);
    //    walletProxy->SetDaemonAddress(DAEMON_ADDR);

    //    rpc_command::WALLET_PAY::request in;
    //    rpc_command::WALLET_PAY::response out;

    //    in.Account = wallet_account;
    //    in.Amount  = AMOUNT_10_GRF;
    //    in.POSAddress = DST_WALLET_ADDR;
    //    in.PaymentID = "1234567890";
    //    in.POSSaleDetails = "1234567890";
    //    in.BlockNum = servant->GetCurrentBlockHeight();
    //    ASSERT_TRUE(in.BlockNum > 0);

    //    ASSERT_TRUE(walletProxy->Pay(in, out));
    string walletProxyPort =  "7500";
    string posProxyPort =  "8500";
    string test_data_path = epee::string_tools::get_current_module_folder() + "/../data/supernode";
    Supernode wallet_proxy(test_data_path);
    wallet_proxy.Start(walletProxyPort, posProxyPort, false);

    Supernode pos_proxy(test_data_path);
    pos_proxy.Start(walletProxyPort, posProxyPort, true);

    while(!wallet_proxy.Started || !pos_proxy.Started)
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    sleep(1);

    bool result = testWalletProxyPay();

    wallet_proxy.Stop();
    pos_proxy.Stop();
    ASSERT_TRUE(result);

}




