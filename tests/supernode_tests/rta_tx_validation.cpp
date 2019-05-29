// Copyright (c) 2019, The Graft Project
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
// Parts of this file are originally copyright (c) 2014-2019 The Monero Project

#include "gtest/gtest.h"

#include "cryptonote_core/tx_pool.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "common/rta_kit.h"

#include <string>

#include <cstdint>
using u32 = std::uint32_t;
using i32 = std::int32_t;

using cryptonote::rta_header;
using cryptonote::rta_signature;

crypto::hash get_txid(void)
{
  const std::string str_txid = "199ea366ca190788e12e7849c9625dd7cad33d2b926de10294005fc20cd41576";
  crypto::hash txid;
  epee::string_tools::hex_to_pod(str_txid, txid);
  return txid;
}

using namespace rta::flow2::validation;

TEST(RtaTxValidationFlow2, check_rta_sign_count)
{
  std::vector<rta_signature> rs;
  for(u32 i = 0, cnt = 5; i < cnt; ++i)
    rs.emplace_back(rta_signature());

  ASSERT_FALSE(check_rta_sign_count(rs, get_txid()));

  rs.emplace_back(rta_signature());
  ASSERT_TRUE(check_rta_sign_count(rs, get_txid()));

  rs.emplace_back(rta_signature());
  ASSERT_TRUE(check_rta_sign_count(rs, get_txid()));

  rs.emplace_back(rta_signature());
  ASSERT_TRUE(check_rta_sign_count(rs, get_txid()));

  rs.emplace_back(rta_signature());
  ASSERT_FALSE(check_rta_sign_count(rs, get_txid()));
}

TEST(RtaTxValidationFlow2, calc_offset_to_supernodes_pub_keys_in_rta_hdr)
{
  rta_header rh;
  for(u32 i = 0, cnt = 8; i < cnt; ++i)
    rh.keys.emplace_back(crypto::public_key());

  ASSERT_EQ(get_auth_sample_public_key_offset(rh), 0);

  for(u32 i = 0, cnt = 3; i < cnt; ++i)
    rh.keys.emplace_back(crypto::public_key());

  ASSERT_EQ(get_auth_sample_public_key_offset(rh), 3);
}

TEST(RtaTxValidationFlow2, check_rta_keys_count)
{
  rta_header rh;
  for(u32 i = 0, cnt = 8; i < cnt; ++i)
  {
    const u32 key_cnt = rh.keys.size();
    if((key_cnt == 8) || (key_cnt == (3 + 8)))
      ASSERT_TRUE(check_rta_keys_count(rh, get_txid()));
    else
      ASSERT_FALSE(check_rta_keys_count(rh, get_txid()));
  }
}

TEST(RtaTxValidationFlow2, check_rta_sign_key_indexes)
{
  std::vector<rta_signature> rs;
  rs.emplace_back(rta_signature());
  auto& sign = rs.back();

  sign.key_index = 0;
  ASSERT_TRUE(check_rta_sign_key_indexes(rs, get_txid(), 0));
  ASSERT_FALSE(check_rta_sign_key_indexes(rs, get_txid(), 1));

  sign.key_index = 3;
  ASSERT_TRUE(check_rta_sign_key_indexes(rs, get_txid(), 3));

  sign.key_index = 7;
  ASSERT_TRUE(check_rta_sign_key_indexes(rs, get_txid(), 3));

  sign.key_index = 10;
  ASSERT_TRUE(check_rta_sign_key_indexes(rs, get_txid(), 3));
  
  sign.key_index = 11;
  ASSERT_FALSE(check_rta_sign_key_indexes(rs, get_txid(), 3));
}

TEST(RtaTxValidationFlow2, check_rta_signatures)
{
  std::vector<rta_signature> rs;
  rta_header rh;

  auto sn_pkeys_off = get_auth_sample_public_key_offset(rh);
  ASSERT_EQ(sn_pkeys_off, 3);

  // nothing to check - then ok
  ASSERT_TRUE(check_rta_signatures(rs, rh, get_txid(), sn_pkeys_off));

  // create case 'out of index' error
  rs.emplace_back(rta_signature());
  ASSERT_FALSE(check_rta_signatures(rs, rh, get_txid(), sn_pkeys_off));

  sn_pkeys_off = 0;

  crypto::hash txid;
  epee::string_tools::hex_to_pod("57fd3427123988a99aae02ce20312b61a88a39692f3462769947467c6e4c3961", txid);

  rh.keys.emplace_back(crypto::public_key());
  auto& pkey = rh.keys.back();
  epee::string_tools::hex_to_pod("a5e61831eb296ad2b18e4b4b00ec0ff160e30b2834f8d1eda4f28d9656a2ec75", pkey);

  auto& sign = rs.back();
  sign.key_index = 0;
  epee::string_tools::hex_to_pod("cd89c4cbb1697ebc641e77fdcd843ff9b2feaf37cfeee078045ef1bb8f0efe0b", sign.signature.c);
  epee::string_tools::hex_to_pod("b5fd0131fbc314121d9c19e046aea55140165441941906a757e574b8b775c008", sign.signature.r);
  ASSERT_TRUE(check_rta_signatures(rs, rh, txid, sn_pkeys_off));

  epee::string_tools::hex_to_pod("92c1259cddde43602eeac1ab825dc12ffc915c9cfe57abcca04c8405df338359", txid);
  epee::string_tools::hex_to_pod("9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f3dc", pkey);
  epee::string_tools::hex_to_pod("b027582f0d05bacb3ebe4e5f12a8a9d65e987cc1e99b759dca3fee84289efa51", sign.signature.c);
  epee::string_tools::hex_to_pod("24ad37550b985ed4f2db0ab6f44d2ebbc195a7123fd39441d3a57e0f70ecf608", sign.signature.r);
  ASSERT_FALSE(check_rta_signatures(rs, rh, txid, sn_pkeys_off));
}

TEST(RtaTxValidationFlow2, belongs_to_auth_sample)
{
  static const std::vector<std::string> auth_sample_pkeys =
  {
    "9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f3dc"
  , "9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f301"
  , "9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f302"
  , "9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f303"
  , "9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f304"
  , "9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f305"
  , "9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f306"
  , "9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a6f307"
  };

  std::vector<crypto::public_key> ask;
  for(const auto& k : auth_sample_pkeys)
  {
    crypto::public_key pk;
    epee::string_tools::hex_to_pod(k, pk);
    ask.emplace_back(pk);
  }

  rta_header rh;
  rh.keys = ask;

  const auto auth_sample_pkeys_off = get_auth_sample_public_key_offset(rh);
  ASSERT_TRUE(belongs_to_auth_sample(ask, rh, auth_sample_pkeys_off));

  epee::string_tools::hex_to_pod("9fa6c7fd338517c7d45b3693fbc91d4a28cd8cc226c4217f3e2694ae89a60000", rh.keys[7]);
  ASSERT_FALSE(belongs_to_auth_sample(ask, rh, auth_sample_pkeys_off));
}

