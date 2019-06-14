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

using namespace epee;
using namespace cryptonote;

namespace
{


}

gen_rta_tests::gen_rta_tests()
{

  REGISTER_CALLBACK_METHOD(gen_rta_tests, check_supernode_stake1);

}

bool gen_rta_tests::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner);
  GENERATE_ACCOUNT(alice);

  MAKE_GENESIS_BLOCK(events, blk_0, miner, ts_start);
  MAKE_ACCOUNT(events, miner0);
  MAKE_ACCOUNT(events, alice0);
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner0);
  MAKE_NEXT_BLOCK(events, blk_1_side, blk_0, miner0); // alternative chain
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner0);
  //MAKE_TX(events, tx_0, first_miner_account, alice, 151, blk_2);

#if 0
  ////  works like this - alice has a 70 coins in
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
  DO_CALLBACK(events, "verify_callback_1");
  REWIND_BLOCKS(events, blk_2r, blk_2, miner0);
  DO_CALLBACK(events, "verify_callback_1");
  transaction tx_0(construct_tx_with_fee(events, blk_2, miner0, alice0, MK_COINS(1000), TESTS_DEFAULT_FEE));
  MAKE_NEXT_BLOCK_TX1(events, blk_22, blk_2r, miner0, tx_0);


  return true;

}

bool gen_rta_tests::check_supernode_stake1(core &c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  cryptonote::account_base miner0 = boost::get<account_base>(events[1]);
  cryptonote::account_base alice0 = boost::get<account_base>(events[2]);
  std::vector<cryptonote::block> chain;
  map_hash2tx_t mtx;
  std::vector<block> block_list;
  bool r = c.get_blocks(0, 1000 + 2 * CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW, block_list);

  /*bool r = */find_block_chain(events, chain, mtx, get_block_hash(block_list.back()));
  MDEBUG("chain size: " << chain.size());
  MDEBUG("events size: " << events.size());
  MDEBUG("ev_index: " << ev_index);
  MDEBUG("BALANCE = " << print_money(get_balance(miner0, chain, mtx)));
  MDEBUG("BALANCE = " << print_money(get_balance(alice0, chain, mtx)));
  for (const auto & tx : mtx) {
    MDEBUG("tx: " << tx.first);
  }
  return true;
}


