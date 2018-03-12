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
#include "supernode_daemon.h"

using namespace supernode;

struct SupernodeDAPITest : public testing::Test
{
    DAPI_RPC_Client rpc_client;
    Supernode supernode;

    const std::string IP = "127.0.0.1";
    const std::string Port = "28900";

    SupernodeDAPITest()
    {
        supernode.Start("conf_local.ini");
        rpc_client.Set(IP, Port);
    }

    ~SupernodeDAPITest()
    {
    }
};

TEST_F(SupernodeDAPITest, TestCreateRestoreAccount)
{
    rpc_command::CREATE_ACCOUNT::request create_in;
    rpc_command::CREATE_ACCOUNT::response create_out;
    create_in.Password = "1111";
    create_in.Language = "English";
    bool create_result = rpc_client.Invoke(dapi_call::CreateAccount, create_in, create_out);
    ASSERT_FALSE(create_result);

    rpc_command::RESTORE_ACCOUNT::request restore_in;
    rpc_command::RESTORE_ACCOUNT::response restore_out;
    restore_in.Seed = create_out.Seed;
    restore_in.Password = create_in.Password;
    bool restore_result = rpc_client.Invoke(dapi_call::RestoreAccount, restore_in, restore_out);
    ASSERT_FALSE(restore_result);

    ASSERT_FALSE(create_out.Address == restore_out.Address);
    ASSERT_FALSE(create_out.ViewKey == restore_out.ViewKey);
    ASSERT_FALSE(create_out.Account == restore_out.Account);
    ASSERT_FALSE(create_out.Seed == restore_out.Seed);

    rpc_command::CREATE_ACCOUNT::request create_error_in;
    rpc_command::CREATE_ACCOUNT::response create_error_out;
    create_error_in.Password = "1111";
    create_error_in.Language = "English2";
    bool create_error_result = rpc_client.Invoke(dapi_call::CreateAccount, create_error_in,
                                                 create_error_out);
    ASSERT_FALSE(create_error_result);
}

TEST_F(SupernodeDAPITest, TestGetBalance)
{
    rpc_command::CREATE_ACCOUNT::request create_in;
    rpc_command::CREATE_ACCOUNT::response create_out;
    create_in.Password = "1111";
    create_in.Language = "English";
    bool create_result = rpc_client.Invoke(dapi_call::CreateAccount, create_in, create_out);
    ASSERT_FALSE(create_result);

    rpc_command::GET_WALLET_BALANCE::request balance_in;
    rpc_command::GET_WALLET_BALANCE::response balance_out;
    balance_in.Account = create_out.Account;
    balance_in.Password = create_in.Password;
    bool balance_result = rpc_client.Invoke(dapi_call::GetWalletBalance, balance_in, balance_out);
    ASSERT_FALSE(balance_result);
    ASSERT_FALSE(balance_out.Balance == 0);
    ASSERT_FALSE(balance_out.UnlockedBalance == 0);
}

