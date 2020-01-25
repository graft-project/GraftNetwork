// Copyright (c) 2019, Graft Project
// Copyright (c) 2014-2018, The Monero Project
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
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "chaingen.h"
#include "rta_tests.h"
#include "graft_rta_config.h"
#include <ostream>

using namespace epee;
using namespace cryptonote;

namespace
{
// for future use
static const std::pair<const char*, const char*> supernode_keys[] = {
{ "6efc0beb23adbe47b762350ec58e7d5054164b900af1d24bb77456b98062a906",
  "f4957a1914119a20106b94fd88f25dd073f452a5199d493807263169c5b9db0b" },
{ "4ae19dcd25fc3b023cc9ff168df0e5057ad3a6f12571707a455d0929ee35ee18",
  "c7e61be8c0fe0198869dd6515b84b0075c962d5fb597d2cadd724a180ce39307" },
{ "90b39f1406a53bddbc3e78883b418e75ecd4c9f8000a0b53ae881dcc4099f3b7",
  "e02313e01503f1e7398f904fdcf1ab3ac4616161df4d7b8fd8cd3e7d0587b409" },
{ "5a8786b55979d5c5b0b2cc162354f00c39d916583d2b97fe9426a99d1c473932",
  "ebcab40d42a47cf52e5deb7f9aa27f7d92913f32644e5b3f38ac436cb32c2d00" },
{ "ef842787b6733ca14de77db5f3f3a6a1c97e1bc9f046f57d3ed3b19484c541ea",
  "1c78b4f2b33b51ba62595dbcc8225dfddd31afbdcdc9c99f4578433c2e6f4f05" },
{ "be01835610894c1deb35160b683b7e267f93cde3d5a7ccfdcec8e9450c5ea22e",
  "bd9b1ca21340093cc7802f7c69323b3079a3b2b9e248d6313b09e664d15bd40c" },
{ "ad07ebf9a0e68a0333c76f12c2231bb04666d2773c8a7926562bf4e01e369111",
  "994f8f0b9cf31591f8e587eb2af7c2be6bc53624e5dbfc6473758bc1027e360d" },
{ "e8f00430416fa44aa032d0134e2c5f349a917d0f6330ae3ebe9a90586ce152db",
  "17cbe82211fa7eca514b78965ceb9981a85d25fe7e0f516ead4509fc94034804" },
{ "328430dfe107a6086239016b7d40b8b2c9856f6cb067326e77a96870475794fc",
  "898498cac3a0ed7c84ca17dbf58777c05687f576220d7286fcaf9016de24b20c" },
{ "0a84b00abe165eccea53064392202f1ea2df572a5b2adc18dcd20940011df5c1",
  "d22b0808cfaa4fccf05762372aaf2063b047ed02374367d07c04eca57633a400" },
{ "13a6cb678edec88e32b001b1959d2ac545b682156301db1710d07beb806a6d66",
  "5eaa4be628b0112c1622711c956e5c5b1dd2bb8f7f6a7be55c3ae2b09d063308" },
{ "e5d9b010fff2f8570eead1ebdd640663816c6cdafde114a9ba6c7154da7b867e",
  "2db7f9bb92d9f768207ea8532b21c39b08c62baa544e8c1d5985bc32697e2d05" },
{ "33134b18123ba123d7b394ef73f8a84d11e9442cf09017c8bbbb94fd24ab2a04",
  "a71398ba703a9ec7b4ee7e4456ff287e1a091901b5789cf74ec55d265525a10b" },
{ "dd6803f71150d96a3e5a02abfa014547e7e23f10a4d04d48d35288760d91d6b8",
  "4265ff3b0f55b18a4cfa1591c89a09ac9c2d2a4a5ec062e02fde2714c0585c0e" },
{ "ebb359bdac313ed773df8e79dd6fa05f2cac10e1fc47de998bc338eec001288b",
  "49d71ad2603ca2924c0187096fe00cf4e261caff5922b9ff33b9f8606163d00a" },
{ "688b66d57adeb2c8f07bb46b975ac94d90db82096cae5b2c52f3f02f1d3d6243",
  "ea333672055c6133cbbabff55dd3482f19b863eba29d90effc5cc2ab71e1bd04" },
};

std::vector<Supernode> g_supernode_list;

}


gen_rta_tests::gen_rta_tests()
{
  // validation calls own constructor so we can't use members here
  if (g_supernode_list.size() == 0) {
    static const size_t LIST_SIZE = 2;
    for (size_t i = 0; i < LIST_SIZE; ++i) {
      g_supernode_list.push_back(Supernode());
    }
  }
  REGISTER_CALLBACK_METHOD(gen_rta_tests, check_stake_registered);
  REGISTER_CALLBACK_METHOD(gen_rta_tests, check_stake_expired);
}

bool gen_rta_tests::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;

  // create one account
  GENERATE_ACCOUNT(miner);
  // create another account
  GENERATE_ACCOUNT(alice);
  // generate genesis block
  MAKE_GENESIS_BLOCK(events, blk_0, miner, ts_start); // height = 1
  // create another account and add it to 'events'
  MAKE_ACCOUNT(events, miner0);
  MAKE_ACCOUNT(events, alice0);

  // generate one block and add it to the chain
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner0); // height = 2
  // MAKE_NEXT_BLOCK(events, blk_1_side, blk_0, miner0); // alternative chain
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner0); // height = 3
  //MAKE_TX(events, tx_0, first_miner_account, alice, 151, blk_2);


#if 0
  ////  how to send multiple txes in one block
  REWIND_BLOCKS(events, blk_2r, blk_2, miner0);
  MAKE_TX_LIST_START(events, txlist_0, miner0, alice0, MK_COINS(1), blk_2);
  MAKE_TX_LIST(events, txlist_0, miner0, alice0, MK_COINS(2), blk_2);
  MAKE_TX_LIST(events, txlist_0, miner0, alice0, MK_COINS(4), blk_2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2r, miner0, txlist_0);
  REWIND_BLOCKS(events, blk_3r, blk_3, miner0);
  MAKE_TX(events, tx_1, miner0, alice0, MK_COINS(50), blk_3);
  DO_CALLBACK(events, "verify_callback_1");
  return true;
  ////
#endif
  // mine N blocks
  REWIND_BLOCKS_N(events, blk_3, blk_2, miner0, 60); // height = 63
  //cryptonote::block blk_3 = blk_2;

  // create transaction
  transaction tx_0(construct_tx_with_fee(events, blk_3, miner0, alice0, MK_COINS(1000), TESTS_DEFAULT_FEE));
  // add it to the new block followed by 'blk_3', mined by 'miner0'
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner0, tx_0); // height = 64

  const size_t STAKE_PERIOD = 100;
  // deposit stakes
  cryptonote::block prev_block;
  std::list<cryptonote::transaction> stake_txes;
  for (Supernode & sn : g_supernode_list) {
    // create stake transaction
    transaction tx(construct_stake_tx_with_fee(events, blk_4, miner0, sn.account, MK_COINS(50000), TESTS_DEFAULT_FEE,
                                               sn.keys.pkey, sn.signature(), 64 + STAKE_PERIOD));
    stake_txes.push_back(tx);
//    MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4, miner0, tx); // height = 64
//    REWIND_BLOCKS_N(events, blk_6, blk_5, miner0, config::graft::STAKE_VALIDATION_PERIOD); // height = 70;
//    prev_block = blk_6;
  }
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_5, blk_4, miner0, stake_txes);

  MDEBUG("stake tx list constructed");


  REWIND_BLOCKS_N(events, blk_6, blk_5, miner0, config::graft::STAKE_VALIDATION_PERIOD); // height = 70;
  // schedule a 'check_stake_registered' check (checking if stake is registered)
  DO_CALLBACK(events, "check_stake_registered");

  // rewind for 'STAKE_PERIOD' blocks
  REWIND_BLOCKS_N(events, bkl_7, blk_6, miner0, STAKE_PERIOD /* + config::graft::TRUSTED_RESTAKING_PERIOD*/); // TODO: check why TRUSTED_RESTAKING_PERIOD is not applied

  // schedule a 'check_stake_expired' check (checking if stake is expired)
  DO_CALLBACK(events, "check_stake_expired");
  return true;
}

// Not used, just to show how to get balances
bool gen_rta_tests::check1(core &c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_rta_tests::check1");
  cryptonote::account_base miner0 = boost::get<account_base>(events[1]);
  cryptonote::account_base alice0 = boost::get<account_base>(events[2]);
  std::vector<cryptonote::block> chain;
  map_hash2tx_t mtx;
  std::vector<block> block_list;
  bool r = c.get_blocks(0, 5 * CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW, block_list);

  /*bool r = */find_block_chain(events, chain, mtx, get_block_hash(block_list.back()));

  MDEBUG("chain size: " << chain.size());
  MDEBUG("chain height (core): " << c.get_current_blockchain_height());
  MDEBUG("events size: " << events.size());
  MDEBUG("ev_index: " << ev_index);
  MDEBUG("miner BALANCE: " << print_money(get_balance(miner0, chain, mtx)));
  MDEBUG("alice BALANCE: " << print_money(get_balance(alice0, chain, mtx)));

  return true;
}


bool gen_rta_tests::check_stake_registered(core &c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_rta_tests::check_stake_registered");
  cryptonote::account_base miner0 = boost::get<account_base>(events[1]);
  cryptonote::account_base alice0 = boost::get<account_base>(events[2]);
  std::vector<cryptonote::block> chain;
  map_hash2tx_t mtx;
  std::vector<block> block_list;
  bool r = c.get_blocks(0, 5 * CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW, block_list);

  /*bool r = */find_block_chain(events, chain, mtx, get_block_hash(block_list.back()));

  for (const auto & tx : mtx) {
    MDEBUG("tx: " << tx.first);
    std::string supernode_public_id;
    cryptonote::account_public_address supernode_public_address;
    crypto::signature supernode_signature;
    crypto::secret_key tx_secret_key;
    bool is_stake_tx = get_graft_stake_tx_extra_from_extra(*tx.second,
                                                           supernode_public_id,
                                                           supernode_public_address,
                                                           supernode_signature,
                                                           tx_secret_key);
    if (is_stake_tx) {
      MDEBUG(" is stake tx for id: " << supernode_public_id);
    }
  }

  StakeTransactionProcessor * stp = c.get_stake_tx_processor();
  MDEBUG("blockchain height: " << c.get_current_blockchain_height());

  CHECK_NOT_EQ(stp->get_blockchain_based_list(), 0);
  CHECK_EQ(stp->get_blockchain_based_list()->block_height(), block_list.size() - 1);

  MDEBUG("stp->get_blockchain_based_list()->block_height(): " << stp->get_blockchain_based_list()->block_height());

  CHECK_EQ(stp->get_blockchain_based_list()->tiers().size(), 4);
  uint64_t stake_amount = 0;
  for (const auto & tier : stp->get_blockchain_based_list()->tiers()) {
    for (const auto & sn: tier) {
      MDEBUG("sn: " << sn.supernode_public_id << ", stake_amount: " << sn.amount << ", expired at height: " << sn.unlock_time);
      stake_amount += sn.amount;
    }
  }
  CHECK_EQ(stake_amount, MK_COINS(50000) * g_supernode_list.size());

  MDEBUG("stake_tx_storage->get_tx_count: " << stp->get_storage()->get_tx_count());
  MDEBUG("stake_tx_storage->get_last_processed_block_index: " << stp->get_storage()->get_last_processed_block_index());
  return true;
}

bool gen_rta_tests::check_stake_expired(core &c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_rta_tests::check_stake_expired");
  cryptonote::account_base miner0 = boost::get<account_base>(events[1]);
  cryptonote::account_base alice0 = boost::get<account_base>(events[2]);
  std::vector<cryptonote::block> chain;
  map_hash2tx_t mtx;
  std::vector<block> block_list;

  // request count doesn't have to be exact value
  bool r = c.get_blocks(0, 5 * CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW, block_list);

  find_block_chain(events, chain, mtx, get_block_hash(block_list.back()));

  StakeTransactionProcessor * stp = c.get_stake_tx_processor();

  CHECK_EQ(stp->get_blockchain_based_list()->block_height(), block_list.size() - 1);
  MDEBUG("stp->get_blockchain_based_list()->block_height(): " << stp->get_blockchain_based_list()->block_height());
  CHECK_EQ(stp->get_blockchain_based_list()->tiers().size(), 4);
  uint64_t stake_amount = 0;
  for (const auto & tier : stp->get_blockchain_based_list()->tiers()) {
    for (const auto & sn: tier) {
      MDEBUG("sn: " << sn.supernode_public_id << ", stake_amount: " << sn.amount << ", expired at height: " << sn.unlock_time);
      stake_amount += sn.amount;
    }
  }
  CHECK_EQ(stake_amount, 0);
  MDEBUG("stake_tx_storage->get_tx_count: " << stp->get_storage()->get_tx_count());
  MDEBUG("stake_tx_storage->get_last_processed_block_index: " << stp->get_storage()->get_last_processed_block_index());

  return true;
}


