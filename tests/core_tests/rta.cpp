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

/*
#include <vector>
#include <iostream>

#include "include_base_utils.h"

#include "console_handler.h"

#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

#include "chaingen.h"
#include "chaingen_tests_list.h"

using namespace std;

using namespace epee;
using namespace cryptonote;
*/

#include "rta.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

/*
////////
// class one_block;

one_block::one_block()
{
  REGISTER_CALLBACK("verify_1", one_block::verify_1);
}

bool one_block::generate(std::vector<test_event_entry> &events)
{
    uint64_t ts_start = 1338224400;

    MAKE_GENESIS_BLOCK(events, blk_0, alice, ts_start);
    MAKE_ACCOUNT(events, alice);
    DO_CALLBACK(events, "verify_1");

    return true;
}

bool one_block::verify_1(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
    DEFINE_TESTS_ERROR_CONTEXT("one_block::verify_1");

    alice = boost::get<cryptonote::account_base>(events[1]);

    // check balances
    //std::vector<const cryptonote::block*> chain;
    //map_hash2tx_t mtx;
    //CHECK_TEST_CONDITION(find_block_chain(events, chain, mtx, get_block_hash(boost::get<cryptonote::block>(events[1]))));
    //CHECK_TEST_CONDITION(get_block_reward(0) == get_balance(alice, events, chain, mtx));

    // check height
    std::vector<cryptonote::block> blocks;
    std::list<crypto::public_key> outs;
    bool r = c.get_blocks(0, 100, blocks);
    //c.get_outs(100, outs);
    CHECK_TEST_CONDITION(r);
    CHECK_TEST_CONDITION(blocks.size() == 1);
    //CHECK_TEST_CONDITION(outs.size() == blocks.size());
    CHECK_TEST_CONDITION(c.get_blockchain_total_transactions() == 1);
    CHECK_TEST_CONDITION(blocks.back() == boost::get<cryptonote::block>(events[0]));

    return true;
}
*/

////////
// class rta;

gen_rta::gen_rta()
{
  REGISTER_CALLBACK("verify_callback_1", gen_rta::verify_callback_1);
//  REGISTER_CALLBACK("verify_callback_2", gen_simple_chain_001::verify_callback_2);
  REGISTER_CALLBACK("check_stake_proc", gen_rta::check_stake_proc);
}

#define MAKE_TX_MIX_R(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD)                       \
/*  cryptonote::transaction TX_NAME;  */                                                           \
  construct_tx_to_key(VEC_EVENTS, TX_NAME, HEAD, FROM, TO, AMOUNT, TESTS_DEFAULT_FEE, NMIX); \
  VEC_EVENTS.push_back(TX_NAME);

#define MAKE_TX_R(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, HEAD) MAKE_TX_MIX_R(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, 0, HEAD)

#define REWIND_BLOCKS_N_R(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, COUNT)           \
  /*cryptonote::block BLK_NAME; */                                                      \
  {                                                                                   \
    cryptonote::block blk_last = PREV_BLOCK;                                            \
    for (size_t i = 0; i < COUNT; ++i)                                                \
    {                                                                                 \
      MAKE_NEXT_BLOCK(VEC_EVENTS, blk, blk_last, MINER_ACC);                          \
      blk_last = blk;                                                                 \
    }                                                                                 \
    BLK_NAME = blk_last;                                                              \
  }

#define REWIND_BLOCKS_R(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC) REWIND_BLOCKS_N_R(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW)

#define MAKE_ACCOUNT_R(VEC_EVENTS, account) \
  /* cryptonote::account_base account; */ \
  account.generate(); \
  VEC_EVENTS.push_back(account);

#define MAKE_NEXT_BLOCK_TX1_R(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TX1)         \
/*  cryptonote::block BLK_NAME; */                                                          \
  {                                                                                   \
    std::list<cryptonote::transaction> tx_list;                                         \
    tx_list.push_back(TX1);                                                           \
    generator.construct_block(BLK_NAME, PREV_BLOCK, MINER_ACC, tx_list);              \
  }                                                                                   \
  VEC_EVENTS.push_back(BLK_NAME);

bool gen_rta::generate(std::vector<test_event_entry> &events)
{
  //  { 14, 336400, 0, 1558504800 },
//  uint64_t ts_start = 1338224400;
  uint64_t ts_start = 1558504800;

    events.reserve(10000);
    const uint64_t FIRST_BLOCK_REWARD = 17592186044415;
    const uint64_t send_amount = FIRST_BLOCK_REWARD - TESTS_DEFAULT_FEE;

//    const int N = 10; //count of participants
    const int N = 1; //count of participants
//    std::string addr[N];
    crypto::public_key pub[N];
    crypto::secret_key sec[N];
    for(int i=0; i<N; ++i)
    {
      crypto::generate_keys(pub[i], sec[i]);
//      addr[i] = std::to_string(i);
    }

    GENERATE_ACCOUNT(miner);
    MAKE_GENESIS_BLOCK(events, blk_0, miner, ts_start);
    MAKE_NEXT_BLOCK(events, blk__1, blk_0, miner);

    REWIND_BLOCKS(events, blk_1r, blk__1, miner);

    MAKE_NEXT_BLOCK(events, blk_i, blk_1r, miner);

    cryptonote::account_base accounts[N];
/*
    cryptonote::transaction tx_1[N];
    cryptonote::transaction tx_2[N];
    cryptonote::transaction tx_3[N];
    cryptonote::transaction tx_4[N];
*/
    cryptonote::block blk_1[N];
    cryptonote::block blk_2[N];
    for(int i=0; i<N; ++i)
    {
//      GENERATE_ACCOUNT(account);
      MAKE_ACCOUNT(events, account);
//      MAKE_ACCOUNT_R(events, accounts[i]);
//      REWIND_BLOCKS(events, blk_ir, blk_i, account);

//      MAKE_TX(events, tx_1, miner, account, MK_COINS(7), blk_1);
//      MAKE_TX(events, tx_1, miner, account, MK_COINS(4), blk_1r);
//      MAKE_TX(events, tx_1, miner, account, MK_COINS(1), blk_last);
//      MAKE_NEXT_BLOCK_TX1(events, blk_a, blk_last, miner, tx_1);
//      MAKE_NEXT_BLOCK_TX1(events, blk_a, blk_i, miner, tx_1);
/*
      MAKE_TX_LIST_START(events, txs, miner, account, MK_COINS(7), blk_last);
      MAKE_TX_LIST(events, txs, miner, account, MK_COINS(2), blk_last);
      MAKE_TX_LIST
*/
//      MAKE_NEXT_BLOCK(events, blk_m, blk_a, miner);
//      REWIND_BLOCKS(events, blk_lastr, blk_m, miner);
//      MAKE_TX(events, tx, account, account, send_amount, blk_m);
//      MAKE_TX(events, tx, miner, account, MK_COINS(1), blk_ir);
//      REWIND_BLOCKS_R(events, blk_1[i], (i? blk_2[i-1] : blk_i), accounts[i]);
//      REWIND_BLOCKS(events, blk_1[i], (i? blk_2[i-1] : blk_i), accounts[i]);
      REWIND_BLOCKS(events, blk_ir, blk_i, account);
      REWIND_BLOCKS(events, blk_irr, blk_ir, account);
//      MAKE_TX_R(events, tx_1[i], miner, accounts[i], MK_COINS(1), blk_1[i]);
//#define MAKE_TX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, HEAD) MAKE_TX_MIX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, 0, HEAD)
//      MAKE_TX_R(events, tx_1[i], accounts[i], accounts[i], MK_COINS(10), blk_1[i]);
//      construct_tx_to_key(events, tx_1[i], blk_1[i], accounts[i], accounts[i], MK_COINS(1), TESTS_DEFAULT_FEE, 0);
//      tx.version = 2;
//      tx.type = cryptonote::transaction::tx_type_rta;
//      construct_tx_to_key(events, tx, blk_ir, account, account, MK_COINS(1), TESTS_DEFAULT_FEE, 0, cryptonote::transaction::tx_type_rta);
//      construct_tx_to_key(events, tx, blk_ir, account, account, MK_COINS(1), TESTS_DEFAULT_FEE, 0, 0);
//      tx.version = 2;
//      tx.extra
//      std::string id_str = epee::string_tools::pod_to_hex(pub[i]);
      std::vector<uint8_t> extra;
      {//generate extra
        std::string id_str = epee::string_tools::pod_to_hex(pub[i]);
        std::string supernode_public_address_str = cryptonote::get_account_address_as_str(cryptonote::TESTNET, false, accounts[i].get_keys().m_account_address);
        std::string data = supernode_public_address_str + ":" + id_str;
        crypto::hash hash;
        crypto::cn_fast_hash(data.data(), data.size(), hash);
        crypto::signature sign; crypto::generate_signature(hash, pub[i], sec[i], sign);
        cryptonote::transaction tx;
        cryptonote::add_graft_stake_tx_extra_to_extra(tx.extra, id_str, accounts[i].get_keys().m_account_address, sign);
        extra = tx.extra;
      }
      cryptonote::transaction tx;
      construct_tx_to_key(events, tx, blk_ir, account, account, MK_COINS(1), TESTS_DEFAULT_FEE, 0, extra, cryptonote::transaction::tx_type_generic);
      tx.invalidate_hashes();
      crypto::hash hash = cryptonote::get_transaction_hash(tx);
      std::cout << "\n-->> my hash " << epee::string_tools::pod_to_hex(hash) << "\n";
//      tx_1[i].set_hash_valid(
//      cryptonote::add_graft_stake_tx_extra_to_extra(tx.extra, id_str, account.get_keys().m_account_address, )
//      MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_a, account, tx);
//      MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_ir, account, tx);
      events.push_back(tx);
//      MAKE_NEXT_BLOCK_TX1_R(events, blk_2[i], blk_1[i], accounts[i], tx);
      MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_irr, account, tx);
      REWIND_BLOCKS(events, blk_2r, blk_2, account);
      MAKE_NEXT_BLOCK(events, blk_3, blk_2r, account);

      accounts[i] = account;
//      blk_last = blk_lastr;
//      blk_1r = blk_2;
      blk_i = blk_3;
    }

    test_event_entry& tee = events[184];
    cryptonote::transaction& tx_ch = boost::get<cryptonote::transaction&>(tee);
//    cryptonote::transaction& tx_ch = tee;

    DO_CALLBACK(events, "check_stake_proc");
//    DO_CALLBACK(events, "verify_callback_1");
/*
    std::string supernode_public_address_str = cryptonote::get_account_address_as_str(m_wallet->nettype(), is_subaddress, supernode_public_address);
    std::string data = supernode_public_address_str + ":" + supernode_public_id;
    crypto::hash hash;
    crypto::cn_fast_hash(data.data(), data.size(), hash);

    if (!crypto::check_signature(hash, W, supernode_signature))
*/
//    MAKE_TX(events, tx_0, first_miner_account, alice, MK_COINS(10), blk_23);                    //  N1+1
    return true;
}

bool temp(std::vector<test_event_entry> &events)
{
    uint64_t ts_start = 1338224400;

    GENERATE_ACCOUNT(miner);
    GENERATE_ACCOUNT(alice);

    MAKE_GENESIS_BLOCK(events, blk_0, miner, ts_start);
    MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner);
    MAKE_NEXT_BLOCK(events, blk_1_side, blk_0, miner);
    MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner);
/*
    {
    cryptonote::transaction TX_NAME;
    construct_tx_to_key(events, TX_NAME, blk_2, alice, alice, MK_COINS(4), 0, 0);
    events.push_back(TX_NAME);
    }
    DO_CALLBACK(events, "verify_callback_1");
*/

    //MAKE_TX(events, tx_0, first_miner_account, alice, 151, blk_2);

    std::vector<cryptonote::block> chain;
    map_hash2tx_t mtx;
    /*bool r = */find_block_chain(events, chain, mtx, get_block_hash(boost::get<cryptonote::block>(events[3])));
    std::cout << "BALANCE = " << get_balance(miner, chain, mtx) << std::endl;

    DO_CALLBACK(events, "verify_callback_1");

    REWIND_BLOCKS(events, blk_2r, blk_2, miner);
    MAKE_TX_LIST_START(events, txlist_0, miner, alice, MK_COINS(1), blk_2);
    MAKE_TX_LIST(events, txlist_0, miner, alice, MK_COINS(2), blk_2);
    MAKE_TX_LIST(events, txlist_0, miner, alice, MK_COINS(4), blk_2);
    MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2r, miner, txlist_0);
    REWIND_BLOCKS(events, blk_3r, blk_3, miner);
    MAKE_TX(events, tx_1, miner, alice, MK_COINS(50), blk_3);
    MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3r, miner, tx_1);
    REWIND_BLOCKS(events, blk_4r, blk_4, miner);
    MAKE_TX(events, tx_2, miner, alice, MK_COINS(50), blk_4);
    MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4r, miner, tx_2);
    REWIND_BLOCKS(events, blk_5r, blk_5, miner);
    MAKE_TX(events, tx_3, miner, alice, MK_COINS(50), blk_5);
    MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5r, miner, tx_3);

    DO_CALLBACK(events, "verify_callback_1");
    //e.t.c.
    //MAKE_BLOCK_TX1(events, blk_3, 3, get_block_hash(blk_0), get_test_target(), first_miner_account, ts_start + 10, tx_0);
    //MAKE_BLOCK_TX1(events, blk_3, 3, get_block_hash(blk_0), get_test_target(), first_miner_account, ts_start + 10, tx_0);
    //DO_CALLBACK(events, "verify_callback_2");

/*    std::vector<const cryptonote::block*> chain;
    map_hash2tx_t mtx;
    if (!find_block_chain(events, chain, mtx, get_block_hash(blk_6)))
        throw;
    cout << "miner = " << get_balance(first_miner_account, events, chain, mtx) << endl;
    cout << "alice = " << get_balance(alice, events, chain, mtx) << endl;*/

    return true;
}

bool gen_rta::check_stake_proc(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_rta::check_stake_proc");
  const cryptonote::StakeTransactionProcessor& stp = c.get_graft_stake_transaction_processor();
/*
  cryptonote::StakeTransactionProcessor stp(c.get_blockchain_storage());
  stp.init_storages("");
  stp.synchronize();
*/
  auto& tiers = stp.get_tiers(10);
//  CHECK_TEST_CONDITION(c.get_pool_transactions_count() == 1);
  return true;
}

bool gen_rta::verify_callback_1(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_rta::verify_callback_1");
  CHECK_TEST_CONDITION(c.get_pool_transactions_count() == 1);
  return true;
}
