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
#include "wallet/wallet2.h"

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



using namespace tools;

static const uint64_t AMOUNT_10_GRF = 10 * COIN;

struct GraftSplittedFeeTest : public testing::Test
{
    std::string wallet_account1;
    std::string wallet_account2;
    std::string wallet_root_path;
    std::string bdb_path;
    const std::string DAEMON_ADDR = "localhost:28281";
    std::string RECIPIENT_ADDR = "T6T2LeLmi6hf58g7MeTA8i4rdbVY8WngXBK3oWS7pjjq9qPbcze1gvV32x7GaHx8uWHQGNFBy1JCY1qBofv56Vwb26Xr998SE";



    GraftSplittedFeeTest()
    {
//        wallet *wallet1 = new GraftWallet(true, false);
//        e *wallet2 = new GraftWallet(true, false);
        wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
//        string wallet_path1 = wallet_root_path + "/miner_wallet";
//        string wallet_path2 = wallet_root_path + "/stake_wallet";
//        wallet1->load(wallet_path1, "");
//        wallet2->load(wallet_path2, "");
//        RECIPIENT_ADDR = wallet2->address();
////        // serialize test wallets
////        wallet_account1 = wallet1->store_keys_graft("", false);
//        delete wallet1;
////        wallet_account2 = wallet2->store_keys_graft("", false);
//        delete wallet2;
    }

    ~GraftSplittedFeeTest()
    {

    }

};

TEST_F(GraftSplittedFeeTest, SplitFeeTest)
{

    vector<string> auth_sample = {
        "T6SMPhhnQcAGuH74Mc1SwLPHqfjWArgWq9BEHC4n25nAfxMcNHnf6KzHgsQtJGhFjCaYvraFN4QkpKPoLRXTiYx21kZ84cXo7",
        "T6TQq1wVq45ihKHcpXc82cXTqdcX2BF6ed8DjNmbR74XieV59C31SoVCR32zMYwiWv6Y4F8WXMfBQgFxErza4YYV33ZTVBucu",
        "T6SKgE9sUqnEf6y2x26HnWPHALAT1hEFgCw6hfDvNiBnZNUoQsML6iFEXKi19fLTh1bh7P2JjiDqT4AZuQ9GLE3z2jGDugZYM",
        "T6TRpaRPr9HFxZBC6pYj27iKeKnvqUoCS2LNgbkBLabFAwuKBcQ7HRi8BqNDmYaukfhswgqD5KbsiYe7tG8qnkSf1z7MPYTDE",
        "T6SQw3DyNkFBMpfVL9dGtTWaJ9ByzF1M2GgyRnidTbfJJS7i8UCvtiFUs4MQXHf51LTRaCrQ25Ekg2ixJBgYLt4a1WrfPTV3c",
        "T6TvUxr8EkC1xVT3raHancQGARpKRCSo664GZZw4t5UdVq4jkbaXR7h8pit59hhvThRdAekQZ6Q8bQPCej3p6GCL2GYQhJHxh",
        "T6TEMA4tgbJfHmouT4994iiUuHwkNWmUHA89dKiidbPyVK7yQQbyL3FAha2t15h2sudsEELjSxLiL2Pa8CVprSJ51vhEZuhY9",
        "T6SEENMa4aHdLevCYrrYykSkdUpKu5y6qHF6FBhfxn7p9WREjvejCgWeSabyMWT5qUhLZT8LJCUaj1pUcQJyBjrQ13DjCi4Ac"
    };

    tools::wallet2 *wallet = new tools::wallet2(true, false);
    string wallet_path1 = wallet_root_path + "/stake_wallet";
    ASSERT_NO_THROW(wallet->load(wallet_path1, ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    wallet->refresh();
    wallet->merge_destinations(true);
    LOG_PRINT_L0("wallet balance: " <<  cryptonote::print_money(wallet->unlocked_balance()));
    LOG_PRINT_L0("wallet default mixin: " <<  wallet->default_mixin());

//    wallet2::create_transactions_graft(const string &recipient_address, const std::vector<string> &auth_sample, uint64_t amount,
//                                                                       double fee_percent, const uint64_t unlock_time, uint32_t priority,
//                                                                       const std::vector<uint8_t> extra, bool trusted_daemon)
    vector<uint8_t> extra;

    vector<wallet2::pending_tx> ptxv = wallet->create_transactions_graft(RECIPIENT_ADDR, auth_sample, AMOUNT_10_GRF,
                                                                        0.1, 0, 2, extra, true);

    LOG_PRINT_L0("merged destinations: " << wallet->merge_destinations());
    for (const auto & ptx : ptxv) {
        std::cout << "fee: " <<  ptx.fee << std::endl;
        std::cout << "dust: " << ptx.dust << std::endl;
        std::cout << "rctFee: " << ptx.tx.rct_signatures.txnFee << std::endl;
        std::cout << "inputs: " << ptx.tx.vin.size() << std::endl;


        for (const auto &in : ptx.tx.vin) {
            std::cout << "   in: " << boost::get<cryptonote::txin_to_key>(in).amount << std::endl;
        }

        std::cout << "outputs: " << ptx.tx.vout.size() << std::endl;
        for (const auto &out : ptx.tx.vout) {
            std::cout << "   out: " << out.amount << std::endl;
        }

    }

}

