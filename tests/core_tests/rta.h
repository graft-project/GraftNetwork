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
/*
#include "block_reward.h"
#include "block_validation.h"
#include "chain_split_1.h"
#include "chain_switch_1.h"
#include "double_spend.h"
#include "integer_overflow.h"
#include "ring_signature_1.h"
#include "tx_validation.h"
#include "v2_tests.h"
#include "rct.h"
#include "multisig.h"
#include "bulletproofs.h"
*/
#include "bulletproofs.h"
/************************************************************************/
/*                                                                      */
/************************************************************************/
struct gen_rta : public gen_bp_tx_validation_base
{
  bool generate(std::vector<test_event_entry>& events) const;

/*
  //it is for not to break other tests
  bool check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t event_idx, const cryptonote::transaction& tx)
  {
    return !gen_bp_tx_validation_base::check_tx_verification_context(tvc, tx_added, event_idx, tx);
  }
*/
};


template<> struct get_test_options<gen_rta>: public get_test_options<gen_bp_tx_validation_base>
{
};

template<> constexpr uint64_t get_fixed_difficulty<get_test_options<gen_rta>>()
{
  return 1;
}

/////////////////////

class gen_rtaX: public test_chain_unit_base
{
  bool check_stake_proc(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry> &events);
public:
  gen_rtaX();
  bool generate(std::vector<test_event_entry> &events);
  bool verify_callback_1(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry> &events); 
};

template<>
struct get_test_options<gen_rtaX> {
//  const std::pair<uint8_t, uint64_t> hard_forks[4] = {std::make_pair(1, 0), std::make_pair(2, 1), std::make_pair(14, 10), std::make_pair(0, 0)};
//  const std::pair<uint8_t, uint64_t> hard_forks[3] = {std::make_pair(1, 0), std::make_pair(14, 10), std::make_pair(0, 0)};
//  const std::pair<uint8_t, uint64_t> hard_forks[4] = { {1, 0}, {2, 1}, {14, 10}, {0, 0} };
  const std::pair<uint8_t, uint64_t> hard_forks[4] = { {1, 0}, {14, 10}, {0, 0} };
  const cryptonote::test_options test_options = {
    hard_forks
  };
  static constexpr uint64_t fixed_difficulty = 1;
};

template<> constexpr uint64_t get_fixed_difficulty<get_test_options<gen_rtaX>>() { return 1; }
