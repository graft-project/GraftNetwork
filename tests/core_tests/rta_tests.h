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

#pragma once 
#include "chaingen.h"
#include "misc_log_ex.h"
#include <vector>

struct Supernode {
  struct Keypair {
    crypto::public_key pkey;
    crypto::secret_key skey;
  };
  cryptonote::account_base account;
  Keypair keys;
  Supernode()
  {
    account.generate();
    crypto::generate_keys(keys.pkey, keys.skey);
  }
  Supernode(int idx);
  crypto::signature signature()
  {
    std::string address = cryptonote::get_account_address_as_str(
          cryptonote::MAINNET, false, account.get_keys().m_account_address);
    std::string msg = address + ":" + epee::string_tools::pod_to_hex(keys.pkey);
    crypto::signature result;
    sign_message(msg, result);
    return result;
  }
  void sign_message(const std::string& msg, crypto::signature &signature)
  {
    crypto::hash hash;
    crypto::cn_fast_hash(msg.data(), msg.size(), hash);
    crypto::generate_signature(hash, keys.pkey, keys.skey, signature);
  }
};

///////////////////////////////////////////
/// gen_rta_disqualification_test
///

struct gen_rta_disqualification_test : public test_chain_unit_base
{
  using single_callback_t = std::function<bool(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)>;

  gen_rta_disqualification_test();

  // test generator method: here we define the test sequence
  bool generate(std::vector<test_event_entry>& events) const;

private:
  void set_single_callback(std::vector<test_event_entry>& events, const single_callback_t& func) const;
  bool call_single_callback(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  cryptonote::transaction make_disqualification1_transaction(std::vector<test_event_entry>& events, std::vector<std::vector<int>>& tiers, int disq_sn_idx) const;
  cryptonote::transaction make_disqualification2_transaction(std::vector<test_event_entry>& events, std::vector<std::vector<int>>& tiers, std::vector<int> disq_sn_idxs) const;

  bool check_bbl_cnt(cryptonote::core& c, int expected_cnt, uint64_t depth, const std::string& context) const;
  void check_bbl_cnt(const std::string& context, std::vector<test_event_entry>& events, int expected_cnt, uint64_t depth = 0) const;

  mutable int tmp_payment_idx = 0;
};

///////////////////////////////////////////
/// gen_rta_test
///

struct gen_rta_test : public test_chain_unit_base
{
  gen_rta_test();

  // test generator method: here we define the test sequence
  bool generate(std::vector<test_event_entry>& events) const;

  // bool check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t event_idx, const cryptonote::block& blk);

  bool check1(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_stake_registered(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_stake_expired(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
private:
};

// this is how to define hardforks table for the cryptonote::core
//template<>
//struct get_test_options<gen_rta_test> {
//  // first element is hf number, second is the height for this hf; last element in array should be {0,0}
//  const std::pair<uint8_t, uint64_t> hard_forks[2] = {std::make_pair(1, 0), std::make_pair(0, 0)};
//  //const std::pair<uint8_t, uint64_t> hard_forks[3] = {std::make_pair(1, 0), std::make_pair(2, 1), std::make_pair(0, 0)};
//  const cryptonote::test_options test_options = {
//    hard_forks
//  };
//};
