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
#include "supernode/TxPool.h"
#include "supernode/WalletProxy.h"
#include "supernode/graft_wallet2.h"
#include "supernode/api/pending_transaction.h"

#include "wallet/wallet2_api.h"


using namespace supernode;
using namespace tools;
using namespace Monero;

struct TxPoolTest : public testing::Test
{

    GraftWallet2 * wallet = nullptr;

    TxPoolTest()
    {
        wallet = new GraftWallet2(true, false);
        string wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
        string wallet_path = wallet_root_path + "/miner_wallet";
        wallet->load(wallet_path, "");
    }

    ~TxPoolTest()
    {
        // XXX !!! ::store() needs to be called explicitly.
        // otherwise it will be "double spent" error before first tx got mined from pool
        wallet->store();

        delete wallet;
    }


};


TEST_F(TxPoolTest, GetTx)
{
    const std::string DAEMON_ADDR = "localhost:28281";
    const std::string DST_WALLET_ADDR = "T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa";

    ASSERT_TRUE(wallet != nullptr);
    ASSERT_TRUE(wallet->init(DAEMON_ADDR));
    wallet->refresh();
    GraftTxExtra tx_extra;
    uint64_t AMOUNT = 10000000000000; // 10 GRF

    tx_extra.BlockNum = 123;
    tx_extra.PaymentID = "Hello";
    tx_extra.Signs.push_back("T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa");
    tx_extra.Signs.push_back("T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa");
    tx_extra.Signs.push_back("T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa");

    Monero::PendingTransaction * ptx =
            wallet->createTransaction("T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa",
                                      "",
                                      AMOUNT,
                                      0,
                                      tx_extra,
                                      Monero::PendingTransaction::Priority_Medium
                                      );
    ASSERT_TRUE(ptx != nullptr);
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
}




TEST_F(TxPoolTest, TestMoneroTx)
{
    const std::string DAEMON_ADDR = "localhost:28281";
    const std::string DST_WALLET_ADDR = "T6SnKmirXp6geLAoB7fn2eV51Ctr1WH1xWDnEGzS9pvQARTJQUXupiRKGR7czL7b5XdDnYXosVJu6Wj3Y3NYfiEA2sU2QiGVa";
    const std::string WALLET_PATH = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets/miner_wallet";
    uint64_t AMOUNT = 10000000000000;
    WalletManager * wmgr = WalletManagerFactory::getWalletManager();
    wmgr->setDaemonAddress(DAEMON_ADDR);

    Monero::Wallet * wallet1 = wmgr->openWallet(WALLET_PATH, "", true);
    // make sure testnet daemon is running
    ASSERT_TRUE(wallet1->init(DAEMON_ADDR, 0));
    ASSERT_TRUE(wallet1->refresh());
    uint64_t balance = wallet1->balance();
    ASSERT_TRUE(wallet1->status() == Monero::PendingTransaction::Status_Ok);

    const int MIXIN_COUNT = 4;


    Monero::PendingTransaction * transaction = wallet1->createTransaction(DST_WALLET_ADDR,
                                                                             "",
                                                                             AMOUNT,
                                                                             0,
                                                                             Monero::PendingTransaction::Priority_Medium);
    ASSERT_TRUE(transaction->status() == Monero::PendingTransaction::Status_Ok);

    ASSERT_TRUE(wallet1->balance() == balance);
    ASSERT_TRUE(transaction->amount() == AMOUNT);
    std::cout << "sending : " << Wallet::displayAmount(transaction->amount()) << std::endl ;
    ASSERT_TRUE(transaction->commit());

    ASSERT_TRUE(transaction->txid().size() > 0);
    std::cout << "sent, tx id : " << transaction->txid()[0] << std::endl;

    ASSERT_FALSE(wallet1->balance() == balance);
    ASSERT_TRUE(wmgr->closeWallet(wallet1));
}

