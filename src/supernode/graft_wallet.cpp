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
// Parts of this file are originally copyright (c) 2014-2017, The Monero Project


#include "graft_wallet.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "common/json_util.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace tools {

bool GraftWallet::verify(const std::string &message, const std::string &address, const std::string &signature,  cryptonote::network_type nettype)
{
  cryptonote::address_parse_info info;
  if (!cryptonote::get_account_address_from_str(info, nettype, address)) {
    LOG_ERROR("get_account_address_from_str");
    return false;
  }
  return wallet2::verify(message, info.address, signature);
}

/*
std::string GraftWallet::store_keys_graft(const std::string& password, bool watch_only)
{
  std::string account_data;
  cryptonote::account_base account = m_account;

  if (watch_only)
    account.forget_spend_key();
  bool r = epee::serialization::store_t_to_binary(account, account_data);
  CHECK_AND_ASSERT_THROW_MES(r, "failed to serialize wallet keys");

  ///Return only account_data
  return account_data;

  GraftWallet::keys_file_data keys_file_data = boost::value_initialized<GraftWallet::keys_file_data>();

  // Create a JSON object with "key_data" and "seed_language" as keys.
  rapidjson::Document json;
  json.SetObject();
  rapidjson::Value value(rapidjson::kStringType);
  value.SetString(account_data.c_str(), account_data.length());
  json.AddMember("key_data", value, json.GetAllocator());
  if (!seed_language.empty())
  {
    value.SetString(seed_language.c_str(), seed_language.length());
    json.AddMember("seed_language", value, json.GetAllocator());
  }

  rapidjson::Value value2(rapidjson::kNumberType);
  value2.SetInt(watch_only ? 1 :0); // WTF ? JSON has different true and false types, and not boolean ??
  json.AddMember("watch_only", value2, json.GetAllocator());

  value2.SetInt(m_always_confirm_transfers ? 1 :0);
  json.AddMember("always_confirm_transfers", value2, json.GetAllocator());

  value2.SetInt(m_print_ring_members ? 1 :0);
  json.AddMember("print_ring_members", value2, json.GetAllocator());

  value2.SetInt(m_store_tx_info ? 1 :0);
  json.AddMember("store_tx_info", value2, json.GetAllocator());

  value2.SetUint(m_default_mixin);
  json.AddMember("default_mixin", value2, json.GetAllocator());

  value2.SetUint(m_default_priority);
  json.AddMember("default_priority", value2, json.GetAllocator());

  value2.SetInt(m_auto_refresh ? 1 :0);
  json.AddMember("auto_refresh", value2, json.GetAllocator());

  value2.SetInt(m_refresh_type);
  json.AddMember("refresh_type", value2, json.GetAllocator());

  value2.SetUint64(m_refresh_from_block_height);
  json.AddMember("refresh_height", value2, json.GetAllocator());

  value2.SetInt(m_confirm_missing_payment_id ? 1 :0);
  json.AddMember("confirm_missing_payment_id", value2, json.GetAllocator());

  value2.SetInt(m_ask_password ? 1 :0);
  json.AddMember("ask_password", value2, json.GetAllocator());

  value2.SetUint(m_min_output_count);
  json.AddMember("min_output_count", value2, json.GetAllocator());

  value2.SetUint64(m_min_output_value);
  json.AddMember("min_output_value", value2, json.GetAllocator());

  value2.SetInt(cryptonote::get_default_decimal_point());
  json.AddMember("default_decimal_point", value2, json.GetAllocator());

  value2.SetInt(m_merge_destinations ? 1 :0);
  json.AddMember("merge_destinations", value2, json.GetAllocator());

  value2.SetInt(m_confirm_backlog ? 1 :0);
  json.AddMember("confirm_backlog", value2, json.GetAllocator());

  value2.SetInt(m_testnet ? 1 :0);
  json.AddMember("testnet", value2, json.GetAllocator());

  // Serialize the JSON object
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  json.Accept(writer);
  account_data = buffer.GetString();

  // Encrypt the entire JSON object.
  crypto::chacha_key key;
  crypto::generate_chacha_key(password, key);
  std::string cipher;
  cipher.resize(account_data.size());
  keys_file_data.iv = crypto::rand<crypto::chacha_iv>();
  crypto::chacha8(account_data.data(), account_data.size(), key, keys_file_data.iv, &cipher[0]);
  keys_file_data.account_data = cipher;

  std::string buf;
  r = ::serialization::dump_binary(keys_file_data, buf);
  CHECK_AND_ASSERT_THROW_MES(r, "failed to serialize wallet data");

  ///Return all wallet data
  LOG_PRINT_L0(sizeof(buf));
  LOG_PRINT_L0(buf.size());
  return buf;
}

bool GraftWallet::load_keys_graft(const string &data, const string &password)
{
    std::string account_data;
    if (false)
    {
        GraftWallet::keys_file_data keys_file_data;
        // Decrypt the contents
        bool r = ::serialization::parse_binary(data, keys_file_data);
        THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize");
        crypto::chacha_key key;
        crypto::generate_chacha_key(password, key);
        account_data.resize(keys_file_data.account_data.size());
        crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);
    }
    else
    {
        account_data = data;
    }

    // The contents should be JSON if the wallet follows the new format.
    rapidjson::Document json;
    if (json.Parse(account_data.c_str()).HasParseError())
    {
        is_old_file_format = true;
        m_watch_only = false;
        m_always_confirm_transfers = false;
        m_print_ring_members = false;
        m_default_mixin = 0;
        m_default_priority = 0;
        m_auto_refresh = true;
        m_refresh_type = RefreshType::RefreshDefault;
        m_confirm_missing_payment_id = true;
        m_ask_password = true;
        m_min_output_count = 0;
        m_min_output_value = 0;
        m_merge_destinations = false;
        m_confirm_backlog = true;
    }
    else
    {
        if (!json.HasMember("key_data"))
        {
            LOG_ERROR("Field key_data not found in JSON");
            return false;
        }
        if (!json["key_data"].IsString())
        {
            LOG_ERROR("Field key_data found in JSON, but not String");
            return false;
        }
        const char *field_key_data = json["key_data"].GetString();
        account_data = std::string(field_key_data, field_key_data + json["key_data"].GetStringLength());

        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, seed_language, std::string, String, false, std::string());
        if (field_seed_language_found)
        {
            set_seed_language(field_seed_language);
        }
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, watch_only, int, Int, false, false);
        m_watch_only = field_watch_only;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, always_confirm_transfers, int, Int, false, true);
        m_always_confirm_transfers = field_always_confirm_transfers;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, print_ring_members, int, Int, false, true);
        m_print_ring_members = field_print_ring_members;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, store_tx_keys, int, Int, false, true);
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, store_tx_info, int, Int, false, true);
        m_store_tx_info = ((field_store_tx_keys != 0) || (field_store_tx_info != 0));
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_mixin, unsigned int, Uint, false, 0);
        m_default_mixin = field_default_mixin;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_priority, unsigned int, Uint, false, 0);
        if (field_default_priority_found)
        {
            m_default_priority = field_default_priority;
        }
        else
        {
            GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_fee_multiplier, unsigned int, Uint, false, 0);
            if (field_default_fee_multiplier_found)
                m_default_priority = field_default_fee_multiplier;
            else
                m_default_priority = 0;
        }
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, auto_refresh, int, Int, false, true);
        m_auto_refresh = field_auto_refresh;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, refresh_type, int, Int, false, RefreshType::RefreshDefault);
        m_refresh_type = RefreshType::RefreshDefault;
        if (field_refresh_type_found)
        {
            if (field_refresh_type == RefreshFull || field_refresh_type == RefreshOptimizeCoinbase || field_refresh_type == RefreshNoCoinbase)
                m_refresh_type = (RefreshType)field_refresh_type;
            else
                LOG_PRINT_L0("Unknown refresh-type value (" << field_refresh_type << "), using default");
        }
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, refresh_height, uint64_t, Uint64, false, 0);
        m_refresh_from_block_height = field_refresh_height;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_missing_payment_id, int, Int, false, true);
        m_confirm_missing_payment_id = field_confirm_missing_payment_id;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, ask_password, int, Int, false, true);
        m_ask_password = field_ask_password;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_decimal_point, int, Int, false, CRYPTONOTE_DISPLAY_DECIMAL_POINT);
        cryptonote::set_default_decimal_point(field_default_decimal_point);
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, min_output_count, uint32_t, Uint, false, 0);
        m_min_output_count = field_min_output_count;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, min_output_value, uint64_t, Uint64, false, 0);
        m_min_output_value = field_min_output_value;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, merge_destinations, int, Int, false, false);
        m_merge_destinations = field_merge_destinations;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_backlog, int, Int, false, true);
        m_confirm_backlog = field_confirm_backlog;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, testnet, int, Int, false, m_testnet);
        // Wallet is being opened with testnet flag, but is saved as a mainnet wallet
        THROW_WALLET_EXCEPTION_IF(m_testnet && !field_testnet, error::wallet_internal_error, "Mainnet wallet can not be opened as testnet wallet");
        // Wallet is being opened without testnet flag but is saved as a testnet wallet.
        THROW_WALLET_EXCEPTION_IF(!m_testnet && field_testnet, error::wallet_internal_error, "Testnet wallet can not be opened as mainnet wallet");
    }

    const cryptonote::account_keys& keys = m_account.get_keys();
    bool r = epee::serialization::load_t_from_binary(m_account, account_data);
    THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);
    r = r && verify_keys(keys.m_view_secret_key,  keys.m_account_address.m_view_public_key);
    THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);
    if(!m_watch_only)
        r = r && verify_keys(keys.m_spend_secret_key, keys.m_account_address.m_spend_public_key);
    THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);
    return true;
}
*/


} // namespace tools
