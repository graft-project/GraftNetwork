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

//#include <iostream>
//#include <vector>
//#include <atomic>
//#include <functional>
//#include <string>
//#include <chrono>
//#include <thread>
#include "supernode/DAPI_RPC_Client.h"
#include "supernode/graft_defines.h"
#include "supernode_daemon.h"

using namespace supernode;

struct SupernodeDAPITest : public testing::Test
{
    DAPI_RPC_Client rpc_client;
    SupernodeDaemon supernode;
    const std::string IP = "127.0.0.1";
    const std::string Port = "28900";

    SupernodeDAPITest()
    {
        supernode.Start(epee::string_tools::get_current_module_folder() + "/../../../../tests/supernode_tests/conf_local.ini");
        while (!supernode.Started)
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));

        sleep(1);

        rpc_client.Set(IP, Port);
    }

    ~SupernodeDAPITest()
    {
        supernode.Stop();
    }
};

TEST_F(SupernodeDAPITest, TestCreateRestoreAccount)
{
    epee::json_rpc::error err;
    rpc_command::CREATE_ACCOUNT::request create_in;
    rpc_command::CREATE_ACCOUNT::response create_out;
    create_in.Password = "1111";
    create_in.Language = "English";
    bool create_result = rpc_client.Invoke(dapi_call::CreateAccount, create_in, create_out, err);
    ASSERT_TRUE(create_result);

    rpc_command::RESTORE_ACCOUNT::request restore_in;
    rpc_command::RESTORE_ACCOUNT::response restore_out;
    restore_in.Seed = create_out.Seed;
    restore_in.Password = create_in.Password;
    bool restore_result = rpc_client.Invoke(dapi_call::RestoreAccount, restore_in, restore_out, err);
    ASSERT_TRUE(restore_result);

    ASSERT_TRUE(create_out.Address == restore_out.Address);
    ASSERT_TRUE(create_out.ViewKey == restore_out.ViewKey);
    ASSERT_TRUE(create_out.Account != restore_out.Account);
    ASSERT_TRUE(create_out.Seed == restore_out.Seed);

    rpc_command::CREATE_ACCOUNT::request create_error_in;
    rpc_command::CREATE_ACCOUNT::response create_error_out;
    create_error_in.Password = "1111";
    create_error_in.Language = "English2";
    bool create_error_result = rpc_client.Invoke(dapi_call::CreateAccount, create_error_in,
                                                 create_error_out, err);
    ASSERT_FALSE(create_error_result);
    ASSERT_TRUE(err.code == ERROR_LANGUAGE_IS_NOT_FOUND);
}

TEST_F(SupernodeDAPITest, TestGetBalance)
{
    epee::json_rpc::error err;
    rpc_command::CREATE_ACCOUNT::request create_in;
    rpc_command::CREATE_ACCOUNT::response create_out;
    create_in.Password = "1111";
    create_in.Language = "English";
    bool create_result = rpc_client.Invoke(dapi_call::CreateAccount, create_in, create_out, err);
    ASSERT_TRUE(create_result);

    rpc_command::GET_WALLET_BALANCE::request balance_in;
    rpc_command::GET_WALLET_BALANCE::response balance_out;
    epee::json_rpc::error balance_err;
    balance_in.Account = create_out.Account;
    balance_in.Password = create_in.Password;
    bool balance_result = rpc_client.Invoke(dapi_call::GetWalletBalance, balance_in, balance_out,
                                            balance_err, std::chrono::seconds(30));
    ASSERT_TRUE(balance_result);
    ASSERT_TRUE(balance_out.Balance == 0);
    ASSERT_TRUE(balance_out.UnlockedBalance == 0);
}

TEST_F(SupernodeDAPITest, TestRTAFlow)
{
    epee::json_rpc::error err;
    rpc_command::RESTORE_ACCOUNT::request pos_restore_in;
    rpc_command::RESTORE_ACCOUNT::response pos_restore_out;
    pos_restore_in.Seed = "sadness adhesive satin popular losing android fidget tubes album "
                          "entrance alley otherwise using jetting silk shuffled strained sanity "
                          "pancakes jogger eleven dagger agnostic aptitude eleven";
    pos_restore_in.Password = "qwerty";
    bool pos_restore_result = rpc_client.Invoke(dapi_call::RestoreAccount, pos_restore_in,
                                                pos_restore_out, err);
    ASSERT_TRUE(pos_restore_result);

    rpc_command::POS_SALE::request pos_sale_in;
    rpc_command::POS_SALE::response pos_sale_out;
    pos_sale_in.POSAddress = pos_restore_out.Address;
    pos_sale_in.POSViewKey = pos_restore_out.ViewKey;
    pos_sale_in.POSSaleDetails = "test";
    pos_sale_in.Amount = 20000000000;
    bool pos_sale_result = rpc_client.Invoke(dapi_call::Sale, pos_sale_in, pos_sale_out, err);
    ASSERT_TRUE(pos_sale_result);

    rpc_command::POS_GET_SALE_STATUS::request pos_status_in;
    rpc_command::POS_GET_SALE_STATUS::response pos_status_out;
    pos_status_in.PaymentID = pos_sale_out.PaymentID;
    bool pos_status_result = rpc_client.Invoke(dapi_call::GetSaleStatus, pos_status_in,
                                               pos_status_out, err);
    ASSERT_TRUE(pos_status_result);
    ASSERT_TRUE(pos_status_out.Status == 1);

    rpc_command::WALLET_GET_POS_DATA::request wallet_data_in;
    rpc_command::WALLET_GET_POS_DATA::response wallet_data_out;
    wallet_data_in.BlockNum = pos_sale_out.BlockNum;
    wallet_data_in.PaymentID = pos_sale_out.PaymentID;
    bool wallet_data_result = rpc_client.Invoke(dapi_call::WalletGetPosData, wallet_data_in,
                                                wallet_data_out, err);
    ASSERT_TRUE(wallet_data_result);
    ASSERT_TRUE(wallet_data_out.POSSaleDetails == pos_sale_in.POSSaleDetails);

    rpc_command::RESTORE_ACCOUNT::request wallet_restore_in;
    rpc_command::RESTORE_ACCOUNT::response wallet_restore_out;
    wallet_restore_in.Seed = "shelter nirvana dice poverty match mirror dunes wagtail tusks "
                             "fugitive cupcake mayor nudged major oxygen value lending session "
                             "maps bacon basin cafe problems rural shelter";
    wallet_restore_in.Password = "qwerty";
    bool wallet_restore_result = rpc_client.Invoke(dapi_call::RestoreAccount, wallet_restore_in,
                                                wallet_restore_out, err);
    ASSERT_TRUE(wallet_restore_result);

    rpc_command::WALLET_PAY::request wallet_pay_in;
    rpc_command::WALLET_PAY::response wallet_pay_out;
    wallet_pay_in.Account = wallet_restore_out.Account;
    wallet_pay_in.Password = wallet_restore_in.Password;
    wallet_pay_in.POSAddress = pos_sale_in.POSAddress;
    wallet_pay_in.BlockNum = pos_sale_out.BlockNum;
    wallet_pay_in.Amount = pos_sale_in.Amount;
    wallet_pay_in.PaymentID = pos_sale_out.PaymentID;
    bool wallet_pay_result = rpc_client.Invoke(dapi_call::Pay, wallet_pay_in, wallet_pay_out, err);
    ASSERT_TRUE(wallet_pay_result);

    rpc_command::WALLET_GET_TRANSACTION_STATUS::request wallet_status_in;
    rpc_command::WALLET_GET_TRANSACTION_STATUS::response wallet_status_out;
    wallet_status_in.PaymentID = wallet_pay_in.PaymentID;
    bool wallet_status_result = rpc_client.Invoke(dapi_call::GetPayStatus, wallet_status_in,
                                                  wallet_status_out, err);
    ASSERT_TRUE(wallet_status_result);
    ASSERT_TRUE(wallet_status_out.Status == 1);
}
