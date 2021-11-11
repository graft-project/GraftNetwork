// Copyright (c) 2017-2018, The Graft Project
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

#include "gtest/gtest.h"

#include <vector>

#include "common/util.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_core/cryptonote_tx_utils.h"


namespace
{
  uint64_t const TEST_FEE = 5000000000; // 5 * 10^9
}


TEST(parse_tx_extra, handles_empty_extra)
{
  std::vector<uint8_t> extra;;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_TRUE(tx_extra_fields.empty());
}

TEST(parse_tx_extra, handles_padding_only_size_1)
{
  const uint8_t extra_arr[] = {0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(1, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_size_2)
{
  const uint8_t extra_arr[] = {0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(2, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_max_size)
{
  std::vector<uint8_t> extra(TX_EXTRA_NONCE_MAX_COUNT, 0);
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[0].type());
  ASSERT_EQ(TX_EXTRA_NONCE_MAX_COUNT, boost::get<cryptonote::tx_extra_padding>(tx_extra_fields[0]).size);
}

TEST(parse_tx_extra, handles_padding_only_exceed_max_size)
{
  std::vector<uint8_t> extra(TX_EXTRA_NONCE_MAX_COUNT + 1, 0);
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
}

TEST(parse_tx_extra, handles_invalid_padding_only)
{
  std::vector<uint8_t> extra(2, 0);
  extra[1] = 42;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
}

TEST(parse_tx_extra, handles_pub_key_only)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
}

TEST(parse_tx_extra, handles_extra_nonce_only)
{
  const uint8_t extra_arr[] = {2, 1, 42};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_nonce), tx_extra_fields[0].type());
  cryptonote::tx_extra_nonce extra_nonce = boost::get<cryptonote::tx_extra_nonce>(tx_extra_fields[0]);
  ASSERT_EQ(1, extra_nonce.nonce.size());
  ASSERT_EQ(42, extra_nonce.nonce[0]);
}

TEST(parse_tx_extra, handles_pub_key_and_padding)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_TRUE(cryptonote::parse_tx_extra(extra, tx_extra_fields));
  ASSERT_EQ(2, tx_extra_fields.size());
  ASSERT_EQ(typeid(cryptonote::tx_extra_pub_key), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(cryptonote::tx_extra_padding), tx_extra_fields[1].type());
}

TEST(parse_and_validate_tx_extra, is_valid_tx_extra_parsed)
{
  cryptonote::transaction tx{};
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::blobdata b = "dsdsdfsdfsf";
  ASSERT_TRUE(cryptonote::construct_miner_tx(0, 0, 10000000000000, 1000, TEST_FEE, acc.get_keys().m_account_address, tx, b));
  crypto::public_key tx_pub_key = cryptonote::get_tx_pub_key_from_extra(tx);
  ASSERT_NE(tx_pub_key, crypto::null_pkey);
}
TEST(parse_and_validate_tx_extra, fails_on_big_extra_nonce)
{
  cryptonote::transaction tx{};
  cryptonote::account_base acc;
  acc.generate();
  cryptonote::blobdata b(TX_EXTRA_NONCE_MAX_COUNT + 1, 0);
  ASSERT_FALSE(cryptonote::construct_miner_tx(0, 0, 10000000000000, 1000, TEST_FEE, acc.get_keys().m_account_address, tx, b));
}

TEST(parse_and_validate_tx_extra, fails_on_wrong_size_in_extra_nonce)
{
  cryptonote::transaction tx{};
  tx.extra.resize(20, 0);
  tx.extra[0] = TX_EXTRA_NONCE;
  tx.extra[1] = 255;
  std::vector<cryptonote::tx_extra_field> tx_extra_fields;
  ASSERT_FALSE(cryptonote::parse_tx_extra(tx.extra, tx_extra_fields));
}
TEST(validate_parse_amount_case, validate_parse_amount)
{
  // some test re-set it with 12;
  cryptonote::set_default_decimal_point(CRYPTONOTE_DISPLAY_DECIMAL_POINT);
  uint64_t res = 0;

  bool r = cryptonote::parse_amount(res, "0.0001");
  ASSERT_TRUE(r);

  ASSERT_EQ(res, 1000000);

  r = cryptonote::parse_amount(res, "100.0001");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 1000001000000);

  r = cryptonote::parse_amount(res, "000.0000");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 0);

  r = cryptonote::parse_amount(res, "0");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 0);


  r = cryptonote::parse_amount(res, "   100.0001    ");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 1000001000000);

  r = cryptonote::parse_amount(res, "   100.0000    ");
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 1000000000000);

  r = cryptonote::parse_amount(res, "   100. 0000    ");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100. 0000");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100 . 0000");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "100.00 00");
  ASSERT_FALSE(r);

  r = cryptonote::parse_amount(res, "1 00.00 00");
  ASSERT_FALSE(r);
}

//TODO: Temporary disable before merging of supernode
#if 0
TEST(parse_tx_extra, handles_graft_tx_extra)
{
    cryptonote::transaction tx = AUTO_VAL_INIT(tx);
    supernode::GraftTxExtra graft_tx_extra1;

    graft_tx_extra1.BlockNum = 123;
    graft_tx_extra1.PaymentID = "1234567890";

    for (int i = 0; i < 100; ++i) {
        graft_tx_extra1.Signs.push_back("SigV1iVT74Y4h8LVLj3WA3HtXHEgaWBvVxVvZwcjbykkSJwjM2rqFPNUWA8JH2QRnpMCHJv8QSe4oi62t58BWBXT1BGor");
    }


    ASSERT_TRUE(cryptonote::add_graft_tx_extra_to_extra(tx, graft_tx_extra1));
    supernode::GraftTxExtra graft_tx_extra2;
    ASSERT_TRUE(cryptonote::get_graft_tx_extra_from_extra(tx, graft_tx_extra2));
    ASSERT_EQ(graft_tx_extra1, graft_tx_extra2);

}
#endif

//TODO: Temporary disable before merging of supernode
#if 0
TEST(parse_tx_extra, handles_graft_tx_extra_and_pubkey)
{
  cryptonote::transaction tx {};
    cryptonote::account_base acc;
    acc.generate();
    cryptonote::blobdata b = "dsdsdfsdfsf";
    // NOTE 1. construct_miner_tx clears extra
    // NOTE 2. this is just for test, nosense in real world - we wont be adding graft extra fields into miner tx
    ASSERT_TRUE(cryptonote::construct_miner_tx(0, 0, 10000000000000, 1000, TEST_FEE, acc.get_keys().m_account_address, tx, b, 1));
    supernode::GraftTxExtra graft_tx_extra1;
    graft_tx_extra1.BlockNum = 123;
    graft_tx_extra1.PaymentID = "1234567890";
    for (int i = 0; i < 100; ++i) {
        graft_tx_extra1.Signs.push_back("SigV1iVT74Y4h8LVLj3WA3HtXHEgaWBvVxVvZwcjbykkSJwjM2rqFPNUWA8JH2QRnpMCHJv8QSe4oi62t58BWBXT1BGor");
    }
    ASSERT_TRUE(cryptonote::add_graft_tx_extra_to_extra(tx, graft_tx_extra1));
    crypto::public_key tx_pub_key = cryptonote::get_tx_pub_key_from_extra(tx);
    ASSERT_NE(tx_pub_key, crypto::null_pkey);
    supernode::GraftTxExtra graft_tx_extra2;
    ASSERT_TRUE(cryptonote::get_graft_tx_extra_from_extra(tx, graft_tx_extra2));
    ASSERT_EQ(graft_tx_extra1, graft_tx_extra2);
}
#endif

TEST(parse_tx_extra, handles_rta_header)
{
  cryptonote::transaction tx = {};
    cryptonote::account_base acc;
    acc.generate();
    cryptonote::blobdata b = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    ASSERT_TRUE(cryptonote::construct_miner_tx(0, 0, 10000000000000, 1000, TEST_FEE, acc.get_keys().m_account_address, tx, b, 1));
    cryptonote::rta_header rta_hdr_in;
    rta_hdr_in.payment_id = "01234567890";

    for (int i = 0; i < 10; ++i) {
        cryptonote::account_base acc;
        acc.generate();
        rta_hdr_in.keys.push_back(acc.get_keys().m_account_address.m_view_public_key);
    }

    ASSERT_TRUE(cryptonote::add_graft_rta_header_to_extra(tx.extra, rta_hdr_in));
    cryptonote::rta_header rta_hdr_out;

    ASSERT_TRUE(cryptonote::get_graft_rta_header_from_extra(tx, rta_hdr_out));
    ASSERT_EQ(rta_hdr_in, rta_hdr_out);
}


TEST(parse_tx_extra, handles_rta_signatures)
{
  cryptonote::transaction tx = {};
    cryptonote::account_base acc;
    acc.generate();

    cryptonote::rta_header rta_hdr_in, rta_hdr_out;
    rta_hdr_in.payment_id = "01234567890";

    std::vector<cryptonote::account_base> accounts;
    for (int i = 0; i < 10; ++i) {
        cryptonote::account_base acc;
        acc.generate();
        rta_hdr_in.keys.push_back(acc.get_keys().m_account_address.m_view_public_key);
        accounts.push_back(acc);
    }

    ASSERT_TRUE(cryptonote::add_graft_rta_header_to_extra(tx.extra, rta_hdr_in));
    ASSERT_TRUE(cryptonote::get_graft_rta_header_from_extra(tx, rta_hdr_out));
    ASSERT_TRUE(rta_hdr_in == rta_hdr_out);
    crypto::hash tx_hash;
    ASSERT_TRUE(cryptonote::get_transaction_hash(tx, tx_hash));

    std::vector<cryptonote::rta_signature> signatures_in, signatures_out;


    for (size_t i = 0; i < 10; ++i) {
        crypto::signature sign;
        crypto::generate_signature(tx_hash, accounts[i].get_keys().m_account_address.m_view_public_key, accounts[i].get_keys().m_view_secret_key, sign);
        signatures_in.push_back({i, sign});
    }
    ASSERT_TRUE(cryptonote::add_graft_rta_signatures_to_extra2(tx.extra2, signatures_in));

    ASSERT_TRUE(cryptonote::get_graft_rta_signatures_from_extra2(tx, signatures_out));
    ASSERT_EQ(signatures_in, signatures_out);

    for (size_t i = 0; i < 10; ++i) {
        const crypto::signature & sign = signatures_out[i].signature;
        ASSERT_TRUE(crypto::check_signature(tx_hash, rta_hdr_out.keys[i], sign));
    }
}

