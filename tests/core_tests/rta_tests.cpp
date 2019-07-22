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
#include "utils/sample_generator.h"
#include "serialization/binary_utils.h"
#include <ostream>

using namespace epee;
using namespace cryptonote;

namespace
{
// for future use
const std::pair<const char*, const char*> supernode_keys[] = {
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

//To provide the same sorting order of sns, supernode_keys is used. It is required for the gen_rta_disqualification_test
Supernode::Supernode(int idx)
{
  account.generate();
  string_tools::hex_to_pod(supernode_keys[idx].first, keys.pkey);
  string_tools::hex_to_pod(supernode_keys[idx].second, keys.skey);
}

///////////////////////////////////////////
/// gen_rta_disqualification_test
///

//Note, ctor of a test is called two times. First time to call generate that collects events and so on. Second time to perform events.
//The workaround for this is using global variables.

std::deque<gen_rta_disqualification_test::single_callback_t> single_callbacks;

void gen_rta_disqualification_test::set_single_callback(std::vector<test_event_entry>& events, const single_callback_t& func) const
{
  single_callbacks.push_back(func);
  DO_CALLBACK(events, "call_single_callback");
}

bool gen_rta_disqualification_test::call_single_callback(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  assert(!single_callbacks.empty());
  bool res = single_callbacks.front()(c, ev_index, events);
  single_callbacks.pop_front();
  return res;
}

gen_rta_disqualification_test::gen_rta_disqualification_test()
{
  REGISTER_CALLBACK_METHOD(gen_rta_disqualification_test, call_single_callback);
}

int get_height(std::vector<test_event_entry>& events)
{
  int i = 0;
  for(auto &e : events) { if(e.which() != 0) continue; ++i; }
  return i;
}

crypto::hash get_block_hash(std::vector<test_event_entry>& events, int blk_idx)
{
  int i = 0;
  for(auto &e : events)
  {
    if(e.which() != 0) continue;
    if(++i == blk_idx)
      return boost::get<cryptonote::block&>(e).hash;
  }
  assert(false);
  return crypto::hash();
}

cryptonote::transaction gen_rta_disqualification_test::make_disqualification1_transaction(std::vector<test_event_entry>& events, std::vector<std::vector<int>>& tiers, int disq_sn_idx) const
{
  std::string str = "something";
  crypto::hash block_hash; //disq_extra.item.block_hash
  crypto::cn_fast_hash(str.data(), str.size(), block_hash);

  std::vector<int> bbqs, qcl;
  bool res = graft::generator::select_BBQS_QCL(block_hash, tiers, bbqs, qcl);
  assert(res);

  int height = get_height(events);

  {
    std::ostringstream oss;
    for(auto i : bbqs) { oss << epee::string_tools::pod_to_hex(g_supernode_list[i].keys.pkey) << "\n"; }
    MWARNING("BBQS: height ") << height << ENDL << oss.str();
  }
  {
    std::ostringstream oss;
    for(auto i : qcl) { oss << epee::string_tools::pod_to_hex(g_supernode_list[i].keys.pkey) << "\n"; }
    MWARNING("QCL: height ") << height << ENDL << oss.str();
  }

  //find index in qcl of disq_sn_idx
  int disq_qcl_idx = -1;
  for(size_t i = 0; i < qcl.size(); ++i)
  {
    if(qcl[i] != disq_sn_idx) continue;
    disq_qcl_idx = i;
    break;
  }
  assert(0 <= disq_qcl_idx);

  //create extra for disqualification1
  std::vector<uint8_t> extra;
  {
    tx_extra_graft_disqualification disq;
    {
      disq.item.block_height = height - 5;
      disq.item.block_hash = get_block_hash(events, disq.item.block_height);
      disq.item.id = g_supernode_list[disq_sn_idx].keys.pkey;
    }
    crypto::hash hash;
    {
      std::string item_str;
      ::serialization::dump_binary(disq.item, item_str);
      crypto::cn_fast_hash(item_str.data(), item_str.size(), hash);
    }
    for(int i = 0, ei = bbqs.size(); i < ei; ++i)
    {
      if(bbqs[i] == disq_sn_idx) continue;
      Supernode& sn = g_supernode_list[bbqs[i]];
      tx_extra_graft_disqualification::signer_item si;
      si.signer_id = sn.keys.pkey;
      MDEBUG("signed id = " ) << epee::string_tools::pod_to_hex(si.signer_id);;
      crypto::generate_signature(hash, sn.keys.pkey, sn.keys.skey, si.sign);
      disq.signers.emplace_back(std::move(si));
    }

    extra.push_back(TX_EXTRA_GRAFT_DISQUALIFICATION_TAG);
    std::string disq_str;
    ::serialization::dump_binary(disq, disq_str);
    std::copy(disq_str.begin(), disq_str.end(), std::back_inserter(extra));
  }

  transaction tx;
  tx.type = 0;
  tx.version = 123;
  tx.extra = extra;

  events.push_back(tx);

  return tx;
}

cryptonote::transaction gen_rta_disqualification_test::make_disqualification2_transaction(std::vector<test_event_entry>& events, std::vector<std::vector<int>>& tiers, std::vector<int> disq_sn_idxs) const
{
  std::string str = "something";
  crypto::hash block_hash;
  crypto::cn_fast_hash(str.data(), str.size(), block_hash);

  std::string payment_id = "payment_id " + std::to_string(++tmp_payment_idx);

  std::vector<int> auths;
  bool res = graft::generator::select_AuthSample(payment_id, tiers, auths);
  assert(res);

  //check that all disq_sn_idxs in auths
  for(auto& idx : disq_sn_idxs)
  {
    auto it = std::find(auths.begin(), auths.end(), idx);
    if(it == auths.end())
    {
      MDEBUG("error, disq_sn_idxs index ") << idx << " is not in auths";
    }
    assert(it != auths.end());
  }

  int height = get_height(events);

  //create extra for disqualification2
  std::vector<uint8_t> extra;
  {
    tx_extra_graft_disqualification2 disq;
    {
      disq.item.payment_id = payment_id;
      disq.item.block_height = height - 5; // - 10;
      disq.item.block_hash = get_block_hash(events, disq.item.block_height);
      for(auto& idx : disq_sn_idxs)
      {
        disq.item.ids.push_back(g_supernode_list[idx].keys.pkey);
      }
    }
    crypto::hash hash;
    {
      std::string item_str;
      ::serialization::dump_binary(disq.item, item_str);
      crypto::cn_fast_hash(item_str.data(), item_str.size(), hash);
    }
    for(int i = 0, ei = auths.size(); i < ei; ++i)
    {
      if(std::find(disq_sn_idxs.begin(), disq_sn_idxs.end(), auths[i]) != disq_sn_idxs.end()) continue;
      Supernode& sn = g_supernode_list[auths[i]];
      tx_extra_graft_disqualification2::signer_item si;
      si.signer_id = sn.keys.pkey;
      MDEBUG("signed id = " ) << epee::string_tools::pod_to_hex(si.signer_id);;
      crypto::generate_signature(hash, sn.keys.pkey, sn.keys.skey, si.sign);
      disq.signers.emplace_back(std::move(si));
    }

    extra.push_back(TX_EXTRA_GRAFT_DISQUALIFICATION2_TAG);
    std::string disq_str;
    ::serialization::dump_binary(disq, disq_str);
    std::copy(disq_str.begin(), disq_str.end(), std::back_inserter(extra));
  }

  transaction tx;
  tx.type = 0;
  tx.version = 124;
  tx.extra = extra;

  events.push_back(tx);

  return tx;
}

#if 0
//useful for debug; put 'set_single_callback(events, my_stop);' into generate and set breakpoint here
bool my_stop(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  return true;
}
#endif

bool gen_rta_disqualification_test::generate(std::vector<test_event_entry>& events) const
{
  assert(single_callbacks.empty());
  {//init g_supernode_list
    g_supernode_list.clear();
    constexpr size_t LIST_SIZE = sizeof(supernode_keys)/sizeof(supernode_keys[0]);
    for (size_t i = 0; i < LIST_SIZE; ++i) {
      g_supernode_list.push_back(Supernode(i));
    }
  }

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

  cryptonote::block last_blk;
  {//it is a trick to make many outputs with enough coins in them
    cryptonote::block top_blk = blk_3;

    std::list<cryptonote::transaction> stake_txes;
    uint64_t coinss[] = {MK_COINS(50000000), MK_COINS(40000000), MK_COINS(30000000), MK_COINS(5000000)};
    for(int idx = 0, cnt = 1; idx < 4; ++idx, cnt*=2)
    {
      for(int i = 0; i < cnt; ++i)
      {
        transaction tx_0(construct_tx_with_fee(events, top_blk, miner0, miner0, coinss[idx], TESTS_DEFAULT_FEE));
        stake_txes.push_back(tx_0);
      }
      MAKE_NEXT_BLOCK_TX_LIST(events, next_blk, top_blk, miner0, stake_txes);
      top_blk = next_blk;
      stake_txes.clear();
    }
    last_blk = top_blk;
  }

  {//fix "One of outputs for one of inputs has wrong tx.unlock_time =
    REWIND_BLOCKS_N(events, blk, last_blk, miner0, 120); last_blk = blk;
  }

  std::vector<std::vector<int>> tiers(4);
  //sn idx to age
  std::map<int, int> sn2age;

  //generate stakes
  {
    int b_height = 164;
    MDEBUG("b_height should be " ) << get_height(events); //187


    struct item_stake
    {
      int blk, sn_idx, tier;
      int blk_duration;
    };

    item_stake stake_table[] = {
      { 0,  0, 0,  70 },
      { 0,  1, 0,  70 },
      { 1,  2, 0,  70 },
      { 1,  3, 0,  70 },
      { 1,  4, 1,  70 },
      { 1,  5, 1,  70 },
      { 3,  6, 1,  70 },
      { 3,  7, 2,  70 },
      { 3,  8, 2,  70 },
      { 3,  9, 2, 100 },
      { 4, 10, 3, 100 },
      { 4, 11, 3, 100 },
    };

    const int stake_table_cnt = sizeof(stake_table)/sizeof(stake_table[0]);

    cryptonote::block l_blk = last_blk;

    std::list<cryptonote::transaction> txes;
    int st_idx = 0;
    for(int i = 0; st_idx < stake_table_cnt ; ++i) //i is block index
    {
      assert(st_idx == stake_table_cnt || i <= stake_table[st_idx].blk);
      for(; st_idx < stake_table_cnt && i == stake_table[st_idx].blk; ++st_idx)
      {
        const item_stake& is = stake_table[st_idx];
        Supernode& sn = g_supernode_list[is.sn_idx];
        assert(0 <= is.tier && is.tier <= 3);
        namespace cg = config::graft;
        uint64_t coins = (is.tier == 0)? cg::TIER1_STAKE_AMOUNT : (is.tier == 1)? cg::TIER2_STAKE_AMOUNT : (is.tier == 2)? cg::TIER3_STAKE_AMOUNT : cg::TIER4_STAKE_AMOUNT;
        tiers[is.tier].push_back(is.sn_idx);
        assert(sn2age.find(is.sn_idx) == sn2age.end());
        sn2age[is.sn_idx] = i;
        // create stake transaction
        transaction tx(construct_stake_tx_with_fee(events, last_blk, miner0, sn.account, coins, TESTS_DEFAULT_FEE,
                                                   sn.keys.pkey, sn.signature(), b_height + i + is.blk_duration));
        txes.push_back(tx);
      }

      if(!txes.empty())
      {
        MAKE_NEXT_BLOCK_TX_LIST(events, next_blk, l_blk, miner0, txes);
        l_blk = next_blk;
        txes.clear();
      }
      else
      {//empty block
        MAKE_NEXT_BLOCK(events, next_blk, l_blk, miner0);
        l_blk = next_blk;
      }
    }
    last_blk  = l_blk;
  }

  MDEBUG("stake tx list constructed");
  {//sort valid supernodes by the age of stake
    for(auto& tier : tiers)
    {
      std::sort(tier.begin(), tier.end(), [&sn2age](int i1, int i2)
      {
        return sn2age[i1] < sn2age[i2] || (sn2age[i1] == sn2age[i2] && epee::string_tools::pod_to_hex(g_supernode_list[i1].keys.pkey) < epee::string_tools::pod_to_hex(g_supernode_list[i2].keys.pkey));
      });
    }
  }

  { REWIND_BLOCKS_N(events, blk, last_blk, miner0, 15); last_blk = blk; }
  check_bbl_cnt("point 1", events, 12, 0);
  check_bbl_cnt("point 2", events, 12, 5);

  //generate disqualifications
  {
    struct item_disq
    {
      int blk;
      int disq_type; //1 or 2
      std::vector<int> sn_idxs; //must be single for disq_type 1
    };

    item_disq disq_table[] = {
      { 0, 1, {5} },
      { 0, 1, {9} },
      { 1, 2, {1, 11}},
      { 1, 1, {8} },
      { 2, 2, {0}},
    };

    const int disq_table_cnt = sizeof(disq_table)/sizeof(disq_table[0]);

    cryptonote::block l_blk = last_blk;

    std::list<cryptonote::transaction> txes;
    int dq_idx = 0;
    for(int i = 0; dq_idx < disq_table_cnt ; ++i) //i is block index
    {
      assert(dq_idx == disq_table_cnt || i <= disq_table[dq_idx].blk);
      for(; dq_idx < disq_table_cnt && i == disq_table[dq_idx].blk; ++dq_idx)
      {
        const item_disq& iq = disq_table[dq_idx];
        assert(iq.disq_type == 1 || iq.disq_type == 2);
        transaction tx;
        if(iq.disq_type == 1)
        {
          assert(iq.sn_idxs.size() == 1);
          tx = make_disqualification1_transaction(events, tiers, iq.sn_idxs[0]);
        }
        else
        {
          assert(!iq.sn_idxs.empty());
          tx = make_disqualification2_transaction(events, tiers, iq.sn_idxs);
        }
        txes.push_back(tx);
      }

      if(!txes.empty())
      {
        MAKE_NEXT_BLOCK_TX_LIST(events, next_blk, l_blk, miner0, txes);
        l_blk = next_blk;
        txes.clear();
      }
      else
      {//empty block
        MAKE_NEXT_BLOCK(events, next_blk, l_blk, miner0);
        l_blk = next_blk;
      }
    }
    last_blk  = l_blk;
  }

  { REWIND_BLOCKS_N(events, blk, last_blk, miner0, 7); last_blk = blk; }

  check_bbl_cnt("point 3", events, 6);
  check_bbl_cnt("point 4", events, 6, 5);

  {//it is assumed DISQUALIFICATION_DURATION_BLOCK_COUNT === DISQUALIFICATION2_DURATION_BLOCK_COUNT === 10
    REWIND_BLOCKS_N(events, blk, last_blk, miner0, DISQUALIFICATION_DURATION_BLOCK_COUNT - 7 - 1); last_blk = blk;
  }

  check_bbl_cnt("point 5", events, 8); //two disqualifications has been expired
  check_bbl_cnt("point 6", events, 6, 5);

  { REWIND_BLOCKS_N(events, blk, last_blk, miner0, 1); last_blk = blk; }

  check_bbl_cnt("point 7", events, 11); //+ three disqualifications expired
  check_bbl_cnt("point 8", events, 6, 5);

  { REWIND_BLOCKS_N(events, blk, last_blk, miner0, 1); last_blk = blk; }

  check_bbl_cnt("point 9", events, 12); //all disqualifications expired
  check_bbl_cnt("point 10", events, 8, 2); //two blocks back shoult be the same

  {// rewind for 'STAKE_PERIOD' blocks
    //config::graft::TRUSTED_RESTAKING_PERIOD === 6
    REWIND_BLOCKS_N(events, blk, last_blk, miner0, 54); last_blk = blk;
  }

  check_bbl_cnt("point 11", events, 0, 0);
  check_bbl_cnt("point 12", events, 2, 1); //two last stakes, see stake_table for expired stakes

  MDEBUG("last block height " ) << get_height(events); //275, b_height(164) + 4 + 100 + TRUSTED_RESTAKING_PERIOD(6) = 274

  return true;
}

bool gen_rta_disqualification_test::check_bbl_cnt(cryptonote::core& c, int expected_cnt, uint64_t depth, const std::string& context) const
{
  std::string ctx = "gen_rta_disqualification_test::check_bbl_cnt " + context;
  DEFINE_TESTS_ERROR_CONTEXT(ctx.c_str());
  StakeTransactionProcessor * stp = c.get_stake_tx_processor();
  MDEBUG("check for blockchain height: ") << c.get_current_blockchain_height() - depth << " expected count = " << expected_cnt;

  const auto & tiers = stp->get_blockchain_based_list()->tiers(depth);;
  int cnt = 0;
  for(auto& tier : tiers){ cnt += tier.size(); }
  CHECK_EQ(cnt, expected_cnt);
  return true;
}

void gen_rta_disqualification_test::check_bbl_cnt(const std::string& context, std::vector<test_event_entry>& events, int expected_cnt, uint64_t depth) const
{
  set_single_callback(events, [this, context, expected_cnt, depth](cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)->bool
  {
    return check_bbl_cnt(c, expected_cnt, depth, context);
  });
}

///////////////////////////////////////////
/// gen_rta_tests
///

gen_rta_tests::gen_rta_tests()
{
  REGISTER_CALLBACK_METHOD(gen_rta_tests, check_stake_registered);
  REGISTER_CALLBACK_METHOD(gen_rta_tests, check_stake_expired);
}

bool gen_rta_tests::generate(std::vector<test_event_entry>& events) const
{
  {//init g_supernode_list
    g_supernode_list.clear();
    constexpr const size_t LIST_SIZE = 2;
    for (size_t i = 0; i < LIST_SIZE; ++i) {
      g_supernode_list.push_back(Supernode());
    }
  }

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
