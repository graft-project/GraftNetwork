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

#include "include_base_utils.h"
#include "cryptonote_config.h"
#include "utils/utils.h"
#include "net/http_client.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "wallet/wallet_rpc_server_commands_defs.h"
#include "utils/cryptmsg.h"


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

#include "supernode/graft_wallet2.h"

using namespace supernode;
using namespace tools;
using namespace Monero;


struct GraftWalletTest : public testing::Test
{
    std::string wallet_account1;
    std::string wallet_account2;
    std::string wallet_root_path;
    std::string bdb_path;
    const std::string DAEMON_ADDR = "localhost:28981";


    GraftWalletTest()
    {
        GraftWallet2 * wallet1 = new GraftWallet2(true, false);
        GraftWallet2 *wallet2 = new GraftWallet2(true, false);
        wallet_root_path = epee::string_tools::get_current_module_folder() + "/../data/supernode/test_wallets";
        string wallet_path1 = wallet_root_path + "/miner_wallet";
        string wallet_path2 = wallet_root_path + "/stake_wallet";
        wallet1->load(wallet_path1, "");
        wallet2->load(wallet_path2, "");
        // serialize test wallets
        wallet_account1 = wallet1->store_keys_graft("", false);
        delete wallet1;
        wallet_account2 = wallet2->store_keys_graft("", false);
        delete wallet2;
    }

    ~GraftWalletTest()
    {
    }

};


TEST_F(GraftWalletTest, StoreAndLoadCache)
{
    GraftWallet2 *wallet = new GraftWallet2(true, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    wallet->refresh();

    // store the cache
    boost::filesystem::path temp = boost::filesystem::temp_directory_path();
    temp /= boost::filesystem::unique_path();
    const string cache_filename = temp.native();
    ASSERT_NO_THROW(wallet->store_cache(cache_filename));
    delete wallet;

    boost::system::error_code ignored_ec;
    ASSERT_TRUE(boost::filesystem::exists(cache_filename, ignored_ec));
    // creating new wallet from serialized keys
    wallet = new GraftWallet2(true, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", cache_filename));
    // check if we loaded blocks from cache
    ASSERT_TRUE(wallet->get_blockchain_current_height() > 100);
    std::cout << "cache stored to: " << cache_filename << std::endl;
    boost::filesystem::remove(temp);
    delete wallet;
}

TEST_F(GraftWalletTest, LoadWrongCache)
{
    GraftWallet2 *wallet = new GraftWallet2(true, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    wallet->refresh();

    // store the cache
    boost::filesystem::path temp = boost::filesystem::temp_directory_path();
    temp /= boost::filesystem::unique_path();
    const string cache_filename = temp.native();
    ASSERT_NO_THROW(wallet->store_cache(cache_filename));
    delete wallet;

    boost::system::error_code ignored_ec;
    ASSERT_TRUE(boost::filesystem::exists(cache_filename, ignored_ec));
    // creating new wallet object, try to load cache from different one
    wallet = new GraftWallet2(true, false);
    ASSERT_ANY_THROW(wallet->load_graft(wallet_account2, "", cache_filename));
    boost::filesystem::remove(temp);
    delete wallet;
}

// implemented here; normally we need the same for wallet/wallet2.cpp
TEST_F(GraftWalletTest, UseForkRule)
{
    GraftWallet2 *wallet = new GraftWallet2(true, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    ASSERT_TRUE(wallet->use_fork_rules(2, 0));
    ASSERT_TRUE(wallet->use_fork_rules(4, 0));
    ASSERT_TRUE(wallet->use_fork_rules(5, 0));
    ASSERT_TRUE(wallet->use_fork_rules(6, 0));
    // this will fail on rta testnet as we need to update block version there
    ASSERT_TRUE(wallet->use_fork_rules(7, 0));
    ASSERT_FALSE(wallet->use_fork_rules(8, 0));
    ASSERT_TRUE(wallet->use_fork_rules(2, 10));
    ASSERT_TRUE(wallet->use_fork_rules(4, 10));
    ASSERT_TRUE(wallet->use_fork_rules(5, 10));
    ASSERT_TRUE(wallet->use_fork_rules(6, 10));
    // this will fail on rta testnet as we need to update block version there
    ASSERT_TRUE(wallet->use_fork_rules(7, 10));
    ASSERT_FALSE(wallet->use_fork_rules(8, 10));
}

TEST_F(GraftWalletTest, PendingTxSerialization1)
{
    GraftWallet2 *wallet = new GraftWallet2(true, false);
    ASSERT_NO_THROW(wallet->load_graft(wallet_account1, "", ""));
    // connect to daemon and get the blocks
    wallet->init(DAEMON_ADDR);
    wallet->refresh();
    vector<cryptonote::tx_destination_entry> dsts;

    cryptonote::tx_destination_entry de;
    cryptonote::account_public_address address;
    string addr_s = "FAY4L4HH9uJEokW3AB6rD5GSA8hw9PkNXMcUeKYf7zUh2kNtzan3m7iJrP743cfEMtMcrToW2R3NUhBaoULHWcJT9NQGJzN";
    cryptonote::get_account_address_from_str(address, true, addr_s);
    de.addr = address;
    de.amount = Monero::Wallet::amountFromString("0.123");
    dsts.push_back(de);
    vector<uint8_t> extra;
    vector<tools::GraftWallet2::pending_tx> ptx1 = wallet->create_transactions_2(dsts, 4, 0, 1, extra, true);
    ASSERT_TRUE(ptx1.size() == 1);
    std::ostringstream oss;
    ASSERT_TRUE(wallet->save_tx_signed(ptx1, oss));
    ASSERT_TRUE(oss.str().length() > 0);
    vector<GraftWallet2::pending_tx> ptx2;
    std::string ptx1_serialized = oss.str();
    std::istringstream iss(ptx1_serialized);
    ASSERT_TRUE(wallet->load_tx(ptx2, iss));

    ASSERT_EQ(ptx1.size() , ptx2.size());
    const GraftWallet2::pending_tx & tx1 = ptx1.at(0);
    const GraftWallet2::pending_tx & tx2 = ptx2.at(0);

    string hash1 = epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(tx1.tx));
    string hash2 = epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(tx2.tx));
    ASSERT_EQ(hash1, hash2);
    LOG_PRINT_L0("sending restored tx: " << hash2);
    ASSERT_NO_THROW(wallet->commit_tx(ptx2));

    delete wallet;
}

// TODO: wrong place for this tests, but just for 'quick-n-dirty' purpose
struct UtilsTest : public testing::Test
{

};

TEST_F(UtilsTest, DecodeAmountWithTxKeySuccess)
{
  epee::net_utils::http::http_simple_client http_client;
  using namespace cryptonote;
  using namespace epee;
  http_client.set_server("localhost:28681",  boost::optional<epee::net_utils::http::login>());
  COMMAND_RPC_GET_TRANSACTIONS::request req;
  COMMAND_RPC_GET_TRANSACTIONS::response res;
  std::string txid = "c2243e28d43cf3c7d9b4de8ae3327b7abf648bf66fba407b988f38ae95d859f1";
  std::string wallet_addr = "F8pVph8wtL2RZay5j94D3WWpR7bTsWfmiRoya93f7udXaNXuB4DpEVp2uoupu6KpzTHTKbfdm92fRd6x3mPRfD5ZNxjjwYs";
  std::string tx_key_str = "96acbc90bc4682cbc0984856bc3475c4a89ff9575e7a85ef4f9945ca71c63b0d";

  crypto::secret_key tx_key;
  epee::string_tools::hex_to_pod(tx_key_str, tx_key);

  req.txs_hashes.push_back(txid);
  bool r = (!net_utils::invoke_http_json("/gettransactions", req, res, http_client) || (res.txs.size() != 1 && res.txs_as_hex.size() != 1));
  ASSERT_FALSE(r);

  cryptonote::blobdata tx_data;
  bool ok;
  if (res.txs.size() == 1)
    ok = string_tools::parse_hexstr_to_binbuff(res.txs.front().as_hex, tx_data);
  else
    ok = string_tools::parse_hexstr_to_binbuff(res.txs_as_hex.front(), tx_data);
  ASSERT_TRUE(ok);
  crypto::hash tx_hash, tx_prefix_hash;
  cryptonote::transaction tx;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(tx_data, tx, tx_hash, tx_prefix_hash));

  uint64_t amount = 0;
  account_public_address address;
  get_account_address_from_str(address, true, wallet_addr);
  std::vector<std::pair<size_t, uint64_t>> outputs;
  ASSERT_TRUE(::Utils::get_tx_amount(address, tx_key, tx, outputs, amount));
  MDEBUG("amount: " << amount);
  ASSERT_TRUE(amount > 0);


}


TEST_F(UtilsTest, DecodeAmountWithTxKeyFail)
{
  epee::net_utils::http::http_simple_client http_client;
  using namespace cryptonote;
  using namespace epee;
  http_client.set_server("localhost:28681",  boost::optional<epee::net_utils::http::login>());
  COMMAND_RPC_GET_TRANSACTIONS::request req;
  COMMAND_RPC_GET_TRANSACTIONS::response res;
  std::string txid = "2d1a8ec99e44e8a27cda9668c326cadc2f2eef962d1e99fcb1ac9c143b1336c1";
  std::string wallet_addr = "F8pVph8wtL2RZay5j94D3WWpR7bTsWfmiRoya93f7udXaNXuB4DpEVp2uoupu6KpzTHTKbfdm92fRd6x3mPRfD5ZNxjjwYs";
  std::string tx_key_str = "96acbc90bc4682cbc0984856bc3475c4a89ff9575e7a85ef4f9945ca71c63b0d";

  crypto::secret_key tx_key;
  epee::string_tools::hex_to_pod(tx_key_str, tx_key);

  req.txs_hashes.push_back(txid);
  bool r = (!net_utils::invoke_http_json("/gettransactions", req, res, http_client) || (res.txs.size() != 1 && res.txs_as_hex.size() != 1));
  ASSERT_FALSE(r);

  cryptonote::blobdata tx_data;
  bool ok;
  if (res.txs.size() == 1)
    ok = string_tools::parse_hexstr_to_binbuff(res.txs.front().as_hex, tx_data);
  else
    ok = string_tools::parse_hexstr_to_binbuff(res.txs_as_hex.front(), tx_data);
  ASSERT_TRUE(ok);
  crypto::hash tx_hash, tx_prefix_hash;
  cryptonote::transaction tx;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(tx_data, tx, tx_hash, tx_prefix_hash));

  uint64_t amount = 0;
  account_public_address address;
  get_account_address_from_str(address, true, wallet_addr);
  std::vector<std::pair<size_t, uint64_t>> outputs;
  ASSERT_TRUE(::Utils::get_tx_amount(address, tx_key, tx, outputs, amount));
  MDEBUG("amount: " << amount);
  ASSERT_FALSE(amount > 0);
}


// TODO: wrong place for this tests, but just for 'quick-n-dirty' purpose
struct RtaRpcTest : public testing::Test
{

};

TEST_F(RtaRpcTest, ReturnsRtaTransaction)
{
  epee::net_utils::http::http_simple_client http_client_wallet;
  using namespace cryptonote;
  using namespace epee;
  http_client_wallet.set_server("localhost:28682",  boost::optional<epee::net_utils::http::login>());
  epee::json_rpc::request<tools::wallet_rpc::COMMAND_RPC_TRANSFER_RTA::request> req = AUTO_VAL_INIT(req);
  epee::json_rpc::response<tools::wallet_rpc::COMMAND_RPC_TRANSFER_RTA::response, std::string> res = AUTO_VAL_INIT(res);

  req.method = "create_rta_tx";
  req.jsonrpc = "2.0";
  req.id = epee::serialization::storage_entry(0);

  req.params.graft_payment_id = "test-test-test";
  req.params.priority = 0;
  req.params.mixin = 7;
  req.params.get_tx_key = 1;
  std::vector<crypto::public_key> pub_keys;
  std::vector<crypto::secret_key> pvt_keys;

  for (int i = 0; i < 8; ++i) {
      crypto::public_key pub_key;
      crypto::secret_key pvt_key;
      generate_keys(pub_key, pvt_key);
      pub_keys.push_back(pub_key);
      pvt_keys.push_back(pvt_key);
  }

  req.params.destinations.push_back({200000000000, "F8sHVwypnjw4fVdMvf3iZ5bisEUJ9DcQoZDBuC7aMRMFPCTHrGtPy7CBe5r68qHdjVKgdggg5NGpAD3r6JoQQiMBAoSEo7x"});
  req.params.destinations.push_back({200000000000, "F3udPm1csSbfMeFAm3DY6xFDyNjGJY4oiPMgrYdxvLC8X4Hom2yNa4oWV3nUcPKzU5R898Ds3xCVv5choXmA5zyiDtiiHkd"});
  req.params.destinations.push_back({200000000000, "FAu8g9xL8fRhDK6rQFhdKhWjZiRunC8Nj891VY99r8C5ipcdbAMmniHPfxB7TAEAQx9kSwJ6FugJTMnkPFtaVKobJ8RUP2Y"});
  req.params.destinations.push_back({200000000000, "F7gHVKGJUmH1Bzxq2e379KF5s8BUaSAhrZuQyTMuVKkKWVK9yHBsc47f6Dc5yFoVu12fPp64ooZdf9UDpa5gmp5j2ArzktC"});

  req.params.destinations.push_back({200000000000, "F6sFs6eJQmMeLz5G5EjDNgj2n6dZaPUuqeU4ZYyrdHVJaAVU8BXLcrRDkQswqX1Twp47DH6EBEMp2heUQ5yoi8TC6fnouxP"});
  req.params.destinations.push_back({200000000000, "FC9AP2aforbFauUn1jxvmbVBawVpq3BdPVVsxE2sDu9YNHttprbnKpiMghnbYf2mLMV3vePEV23WvCEi6TCBP6JK7ttytY3"});
  req.params.destinations.push_back({200000000000, "F5T3RSYy34iLrXHHaST9aRVC1nnSrivyA8ockQmTYLDLcybcswwGp8BMNYKmP8k2dqNM3GRQi1BEEfqr5abEJZtH3tuShKL"});

  req.params.destinations.push_back({500000000000, "FBHdqDnz8YNU3TscB1X7eH25XLKaHmba9Vws2xwDtLKJWS7vZrii1vHVKAMGgRVz6WVcd2jN7qC1YEB4ZALUag2d9h1nnEu"});

  for (const crypto::public_key &pkey : pub_keys) {
     req.params.supernode_keys.push_back(epee::string_tools::pod_to_hex(pkey));
  }

  bool r = net_utils::invoke_http_json("/json_rpc", req, res, http_client_wallet);
  ASSERT_TRUE(r);

  cryptonote::transaction tx;
  std::string tx_blob;
  ASSERT_TRUE(string_tools::parse_hexstr_to_binbuff(res.result.tx_blob, tx_blob));
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(tx_blob, tx));

  cryptonote::rta_header rta_hdr_in;
  rta_hdr_in.payment_id = req.params.graft_payment_id;
  rta_hdr_in.keys = pub_keys;

  cryptonote::rta_header rta_hdr_out;
  ASSERT_TRUE(cryptonote::get_graft_rta_header_from_extra(tx, rta_hdr_out));
  ASSERT_EQ(rta_hdr_in, rta_hdr_out);

  std::string tx_key_encrypted;
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(res.result.encrypted_tx_key, tx_key_encrypted));
  std::string tx_key_plain;

  for (int i = 0; i < 8; ++i) {
    ASSERT_TRUE(graft::crypto_tools::decryptMessage(tx_key_encrypted, pvt_keys[i], tx_key_plain));
    crypto::secret_key tx_key_decrypted = *(reinterpret_cast<const crypto::secret_key*>(tx_key_plain.c_str()));
    ASSERT_EQ(epee::string_tools::pod_to_hex(tx_key_decrypted), res.result.tx_key);
  }
}
// TODO: move this to appropriate place, probably even better to move to supernode (graft_server) project
TEST_F(RtaRpcTest, TransfersRtaTransaction)
{
  using namespace cryptonote;
  using namespace epee;
  epee::net_utils::http::http_simple_client http_client_wallet, http_client_daemon;

  http_client_wallet.set_server("localhost:28682",  boost::optional<epee::net_utils::http::login>());
  http_client_daemon.set_server("localhost:28681",  boost::optional<epee::net_utils::http::login>());
  epee::json_rpc::request<tools::wallet_rpc::COMMAND_RPC_TRANSFER_RTA::request> req = AUTO_VAL_INIT(req);
  epee::json_rpc::response<tools::wallet_rpc::COMMAND_RPC_TRANSFER_RTA::response, std::string> res = AUTO_VAL_INIT(res);

  req.method = "create_rta_tx";
  req.jsonrpc = "2.0";
  req.id = epee::serialization::storage_entry(0);

  req.params.graft_payment_id = "test-test-test";
  req.params.auth_sample_height = 243285;
  req.params.priority = 0;
  req.params.mixin = 7;
  req.params.get_tx_key = 1;


  // auth sample
  req.params.destinations.push_back({2000000000, "F8sHVwypnjw4fVdMvf3iZ5bisEUJ9DcQoZDBuC7aMRMFPCTHrGtPy7CBe5r68qHdjVKgdggg5NGpAD3r6JoQQiMBAoSEo7x"});
  req.params.destinations.push_back({2000000000, "F3udPm1csSbfMeFAm3DY6xFDyNjGJY4oiPMgrYdxvLC8X4Hom2yNa4oWV3nUcPKzU5R898Ds3xCVv5choXmA5zyiDtiiHkd"});
  req.params.destinations.push_back({2000000000, "FAu8g9xL8fRhDK6rQFhdKhWjZiRunC8Nj891VY99r8C5ipcdbAMmniHPfxB7TAEAQx9kSwJ6FugJTMnkPFtaVKobJ8RUP2Y"});
  req.params.destinations.push_back({2000000000, "F7gHVKGJUmH1Bzxq2e379KF5s8BUaSAhrZuQyTMuVKkKWVK9yHBsc47f6Dc5yFoVu12fPp64ooZdf9UDpa5gmp5j2ArzktC"});
  // destination wallet
  req.params.destinations.push_back({5000000000, "FBHdqDnz8YNU3TscB1X7eH25XLKaHmba9Vws2xwDtLKJWS7vZrii1vHVKAMGgRVz6WVcd2jN7qC1YEB4ZALUag2d9h1nnEu"});

  std::vector<crypto::public_key> pub_keys;
  {
    crypto::public_key pubkey;
    epee::string_tools::hex_to_pod("cdba49cbdece633266681b3c6f80f1085e7b3d3e0cca395d3986d10ab3ea0d6a", pubkey);
    pub_keys.push_back(pubkey);
    epee::string_tools::hex_to_pod("ce7cf758df6f2eb7f74d28730078be733cb953bda37a5f6e54ab09140f40e712", pubkey);
    pub_keys.push_back(pubkey);
    epee::string_tools::hex_to_pod("25b316d25e6c2dd8dd60fd983de9fbd5a9bb1fcf96d65bbb1c295708bafa00cb", pubkey);
    pub_keys.push_back(pubkey);
    epee::string_tools::hex_to_pod("914c13339fdfacdbbebe4c223d1900415432aab24f1f995823286104c7ac9eaa", pubkey);
    pub_keys.push_back(pubkey);
  }

  std::vector<crypto::secret_key> pvt_keys;
  {
    crypto::secret_key pvtkey;
    epee::string_tools::hex_to_pod("55260a5bf280cc91f9c36105b1dff9fef1559003f144d2fe577de8ba113ffc0b", pvtkey);
    pvt_keys.push_back(pvtkey);
    epee::string_tools::hex_to_pod("574fbb96e8af38f372e95608cab335f1f7d1895735004d1161c51ddba4988f09", pvtkey);
    pvt_keys.push_back(pvtkey);
    epee::string_tools::hex_to_pod("86cb6f1d884b2280c9ec703946b6476888d9aba78ca5e5c6367378c9ca347300", pvtkey);
    pvt_keys.push_back(pvtkey);
    epee::string_tools::hex_to_pod("546164cec18a87a729e83ff7683722ed184d91434638f8a04bf037b13aeb900f", pvtkey);
    pvt_keys.push_back(pvtkey);
  }

  for (const crypto::public_key &pkey : pub_keys) {
     req.params.supernode_keys.push_back(epee::string_tools::pod_to_hex(pkey));
  }

  bool r = net_utils::invoke_http_json("/json_rpc", req, res, http_client_wallet);
  ASSERT_TRUE(r);
  ASSERT_FALSE(res.result.tx_hash.empty());

#if 0
  cryptonote::transaction tx;
  std::string tx_blob;
  ASSERT_TRUE(string_tools::parse_hexstr_to_binbuff(res.result.tx_blob, tx_blob));
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(tx_blob, tx));
  COMMAND_RPC_SEND_RAW_TX::request tx_req;
  COMMAND_RPC_SEND_RAW_TX::response tx_resp;
  // Try to submit tx without rta signatures
  tx_req.tx_as_hex = res.result.tx_blob;
  tx_req.do_not_relay = false;
  r = net_utils::invoke_http_json("/sendrawtransaction", tx_req, tx_resp, http_client_daemon);
  ASSERT_TRUE(r);
  ASSERT_TRUE(tx_resp.rta_validation_failed);

  // try to submit tx with incorrect signatures
  std::string wrong_tx_id_str = res.result.tx_hash;
  for (int i = 0; i < 5; ++i) {
    wrong_tx_id_str[i] = '1';
  }
  crypto::hash wrong_tx_id;
  epee::string_tools::hex_to_pod(wrong_tx_id_str, wrong_tx_id);

  for (size_t i = 0; i < pub_keys.size(); ++i) {
    crypto::signature sign;
    crypto::generate_signature(wrong_tx_id, pub_keys[i], pvt_keys[i], sign);
    tx.rta_signatures.push_back({i, sign});
  }

  cryptonote::blobdata blob;
  tx_to_blob(tx, blob);
  tx_req.tx_as_hex = epee::string_tools::buff_to_hex_nodelimer(blob);
  r = net_utils::invoke_http_json("/sendrawtransaction", tx_req, tx_resp, http_client_daemon);
  ASSERT_TRUE(r);
  ASSERT_TRUE(tx_resp.rta_validation_failed);

  // try to submit tx with correct signatures
  crypto::hash tx_id;
  epee::string_tools::hex_to_pod(res.result.tx_hash, tx_id);
  tx.rta_signatures.clear();
  for (size_t i = 0; i < pub_keys.size(); ++i) {
    crypto::signature sign;
    crypto::generate_signature(tx_id, pub_keys[i], pvt_keys[i], sign);
    tx.rta_signatures.push_back({i, sign});
  }
  blob.clear();
  tx_to_blob(tx, blob);
  tx_req.tx_as_hex = epee::string_tools::buff_to_hex_nodelimer(blob);
  r = net_utils::invoke_http_json("/sendrawtransaction", tx_req, tx_resp, http_client_daemon);
  ASSERT_TRUE(r);
  ASSERT_FALSE(tx_resp.rta_validation_failed);
#endif

}
