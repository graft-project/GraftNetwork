#include "graft_wallet.h"
#include "wallet_errors.h"
#include "serialization/binary_utils.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "common/json_util.h"
#include "string_coding.h"
#include <boost/format.hpp>

#define SUBADDRESS_LOOKAHEAD_MAJOR 50
#define SUBADDRESS_LOOKAHEAD_MINOR 200

#define CACHE_KEY_TAIL 0x8d

using namespace crypto;

namespace
{
  std::string get_default_db_path(cryptonote::network_type nettype)
  {
    boost::filesystem::path dir = tools::get_default_data_dir();
    // remove .bitmonero, replace with .shared-ringdb
    dir = dir.remove_filename();
    dir /= ".shared-ringdb";
    switch (nettype) {
    case cryptonote::TESTNET:
        return (boost::filesystem::path(dir.string()) / "testnet").string();
    case cryptonote::STAGENET:
        return (boost::filesystem::path(dir.string()) / "stagenet").string();
    case cryptonote::MAINNET:
    default:
        return dir.string();
    }
  }
}
/*!
 * \brief Generates a wallet or restores one.
 * \param  password       Password of wallet file
 * \param  recovery_param If it is a restore, the recovery key
 * \param  recover        Whether it is a restore
 * \param  two_random     Whether it is a non-deterministic wallet
 * \return                The secret key of the generated wallet
 */
tools::GraftWallet::GraftWallet(cryptonote::network_type nettype, uint64_t kdf_rounds, bool unattended)
    : wallet2(nettype, kdf_rounds, unattended)
{

}

std::unique_ptr<tools::GraftWallet> tools::GraftWallet::createWallet(const std::string &daemon_address,
                           const std::string &daemon_host, int daemon_port,
                           const std::string &daemon_login, cryptonote::network_type nettype)
{
    //make_basic() analogue
    THROW_WALLET_EXCEPTION_IF(!daemon_address.empty() && !daemon_host.empty() && 0 != daemon_port,
                              tools::error::wallet_internal_error,
                              tools::GraftWallet::tr("can't specify daemon host or port more than once"));
    boost::optional<epee::net_utils::http::login> login{};
    if (!daemon_login.empty())
    {
        std::string ldaemon_login(daemon_login);

        auto parsed = tools::login::parse(std::move(ldaemon_login), false, nullptr);
        if (!parsed)
        {
            return nullptr;
        }
        login.emplace(std::move(parsed->username), std::move(parsed->password).password());
    }
    std::string ldaemon_host = daemon_host.empty() ? "localhost" : daemon_host;
    if (!daemon_port)
    {
        daemon_port = cryptonote::get_config(nettype).RPC_DEFAULT_PORT;
    }
    std::string ldaemon_address = daemon_address;
    if (daemon_address.empty())
    {
        ldaemon_address = std::string("http://") + ldaemon_host + ":" + std::to_string(daemon_port);
    }
    std::unique_ptr<tools::GraftWallet> wallet(new tools::GraftWallet(nettype));
    wallet->init(std::move(ldaemon_address), std::move(login));
    wallet->set_ring_database(get_default_db_path(nettype));
    return wallet;
}

std::unique_ptr<tools::GraftWallet> tools::GraftWallet::createWallet(const std::string &account_data,
                  const std::string &password, const std::string &daemon_address,
                  const std::string &daemon_host, int daemon_port, const std::string &daemon_login,
                  cryptonote::network_type nettype, bool use_base64)
{
    auto wallet = createWallet(daemon_address, daemon_host, daemon_port, daemon_login, nettype);
    if (wallet)
    {
        wallet->loadFromData(account_data, password, "" /*cache_file*/, use_base64);
    }
    return std::move(wallet);
}

bool tools::GraftWallet::verify_message(const std::string &message, const std::string &address,
                                        const std::string &signature, cryptonote::network_type nettype)
{
    cryptonote::address_parse_info addr;
    if (!cryptonote::get_account_address_from_str(addr, nettype, address))
    {
        LOG_PRINT_L0("!get_account_integrated_address_from_str");
        return false;
    }
    return wallet2::verify(message, addr.address, signature);
}

crypto::secret_key tools::GraftWallet::generateFromData(const std::string &password,
                                                         const crypto::secret_key &recovery_param,
                                                         bool recover, bool two_random)
{
    clear();

    crypto::secret_key retval = m_account.generate(recovery_param, recover, two_random);

    m_account_public_address = m_account.get_keys().m_account_address;
    m_watch_only = false;
    m_multisig = false;
    m_multisig_threshold = 0;
    m_multisig_signers.clear();
    m_key_device_type = hw::device::device_type::SOFTWARE;
    setup_cache_keys(password);

    // calculate a starting refresh height
    if(m_refresh_from_block_height == 0 && !recover)
    {
        m_refresh_from_block_height = estimate_blockchain_height();
    }

    setup_new_blockchain();

    return retval;
}

void tools::GraftWallet::loadFromData(const std::string &data, const std::string &password,
                                       const std::string &cache_file, bool use_base64)
{
    clear();
    std::string enc_data = data;
    if (use_base64)
    {
        enc_data = epee::string_encoding::base64_decode(data);
    }
    if (!load_keys_from_data(enc_data, password))
    {
        THROW_WALLET_EXCEPTION_IF(true, error::file_read_error, m_keys_file);
    }
    LOG_PRINT_L0("Loaded wallet keys file, with public address: " << m_account.get_public_address_str(nettype()));

    //keys loaded ok!
    //try to load wallet file. but even if we failed, it is not big problem

    m_account_public_address = m_account.get_keys().m_account_address;

    if (!cache_file.empty())
    {
        load_cache(cache_file);
    }

    cryptonote::block genesis;
    generate_genesis(genesis);
    crypto::hash genesis_hash = get_block_hash(genesis);

    if (m_blockchain.empty())
    {
        m_blockchain.push_back(genesis_hash);
        m_last_block_reward = cryptonote::get_outs_money_amount(genesis.miner_tx);
    }
    else
    {
        check_genesis(genesis_hash);
    }

    trim_hashchain();

    if (get_num_subaddress_accounts() == 0)
      add_subaddress_account(tr("Primary account"));

    try
    {
      find_and_save_rings(false);
    }
    catch (const std::exception &e)
    {
      MERROR("Failed to save rings, will try again next time");
    }
}

void tools::GraftWallet::update_tx_cache(const tools::wallet2::pending_tx &ptx)
{
    using namespace cryptonote;
    crypto::hash txid;

    // sanity checks
    for (size_t idx: ptx.selected_transfers)
    {
        THROW_WALLET_EXCEPTION_IF(idx >= m_transfers.size(), error::wallet_internal_error,
                                  "Bad output index in selected transfers: " + boost::lexical_cast<std::string>(idx));
    }

    txid = get_transaction_hash(ptx.tx);
    crypto::hash payment_id = crypto::null_hash;
    std::vector<cryptonote::tx_destination_entry> dests;
    uint64_t amount_in = 0;
    if (store_tx_info())
    {
        payment_id = get_payment_id(ptx);
        dests = ptx.dests;
        for(size_t idx: ptx.selected_transfers)
        {
            amount_in += m_transfers[idx].amount();
        }
    }
    add_unconfirmed_tx(ptx.tx, amount_in, dests, payment_id, ptx.change_dts.amount, ptx.construction_data.subaddr_account, ptx.construction_data.subaddr_indices);
    if (store_tx_info())
    {
        LOG_PRINT_L2("storing tx key " << ptx.tx_key);
        m_tx_keys.insert(std::make_pair(txid, ptx.tx_key));
        m_additional_tx_keys.insert(std::make_pair(txid, ptx.additional_tx_keys));
        LOG_PRINT_L2("there're  " << m_tx_keys.size() << " stored keys");
    }

    LOG_PRINT_L2("transaction " << txid << " generated ok and sent to daemon, key_images: [" << ptx.key_images << "]");

    for(size_t idx: ptx.selected_transfers)
    {
        set_spent(idx, 0);
    }

    // tx generated, get rid of used k values
    for (size_t idx: ptx.selected_transfers)
    {
        m_transfers[idx].m_multisig_k.clear();
    }
}

std::string tools::GraftWallet::getAccountData(const std::string &password, bool use_base64)
{
    std::string data = store_keys_to_data(password);
    if (use_base64)
    {
        return epee::string_encoding::base64_encode(data);
    }
    return data;
}

std::string tools::GraftWallet::store_keys_to_data(const std::string &password, bool watch_only)
{
    std::string account_data;
    cryptonote::account_base account = m_account;

    if (watch_only)
      account.forget_spend_key();
    bool r = epee::serialization::store_t_to_binary(account, account_data);
    CHECK_AND_ASSERT_THROW_MES(r, "failed to serialize wallet keys");

    ///Return only account_data
    return account_data;
}

bool tools::GraftWallet::load_keys_from_data(const std::string &data, const std::string &password)
{
    rapidjson::Document json;
    std::string account_data;
    bool r = false;
    if (false)
    {
        wallet2::keys_file_data keys_file_data;
        // Decrypt the contents
        bool r = ::serialization::parse_binary(data, keys_file_data);
        THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize");
        crypto::chacha_key key;
        crypto::generate_chacha_key(password, key, m_kdf_rounds);
        account_data.resize(keys_file_data.account_data.size());
        crypto::chacha20(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);
        if (json.Parse(account_data.c_str()).HasParseError() || !json.IsObject())
          crypto::chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);
    }
    else
    {
        account_data = data;
    }

    // The contents should be JSON if the wallet follows the new format.
    if (json.Parse(account_data.c_str()).HasParseError())
    {
        is_old_file_format = true;
        m_watch_only = false;
        m_multisig = false;
        m_multisig_threshold = 0;
        m_multisig_signers.clear();
        m_multisig_rounds_passed = 0;
        m_multisig_derivations.clear();
        m_always_confirm_transfers = false;
        m_print_ring_members = false;
        m_default_mixin = 0;
        m_default_priority = 0;
        m_auto_refresh = true;
        m_refresh_type = RefreshType::RefreshDefault;
        m_confirm_missing_payment_id = true;
        m_confirm_non_default_ring_size = true;
        m_ask_password = AskPasswordToDecrypt;
        m_min_output_count = 0;
        m_min_output_value = 0;
        m_merge_destinations = false;
        m_confirm_backlog = true;
        m_confirm_backlog_threshold = 0;
        m_confirm_export_overwrite = true;
        m_auto_low_priority = true;
        m_segregate_pre_fork_outputs = true;
        m_key_reuse_mitigation2 = true;
        m_segregation_height = 0;
        m_ignore_fractional_outputs = true;
        m_subaddress_lookahead_major = SUBADDRESS_LOOKAHEAD_MAJOR;
        m_subaddress_lookahead_minor = SUBADDRESS_LOOKAHEAD_MINOR;
        m_device_name = "";
        m_key_device_type = hw::device::device_type::SOFTWARE;
    }
    else if (json.IsObject())
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

        if (json.HasMember("key_on_device"))
        {
          GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, key_on_device, int, Int, false, hw::device::device_type::SOFTWARE);
          m_key_device_type = static_cast<hw::device::device_type>(field_key_on_device);
        }

        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, seed_language, std::string, String, false, std::string());
        if (field_seed_language_found)
        {
            set_seed_language(field_seed_language);
        }
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, watch_only, int, Int, false, false);
        m_watch_only = field_watch_only;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, multisig, int, Int, false, false);
        m_multisig = field_multisig;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, multisig_threshold, unsigned int, Uint, m_multisig, 0);
        m_multisig_threshold = field_multisig_threshold;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, multisig_rounds_passed, unsigned int, Uint, false, 0);
        m_multisig_rounds_passed = field_multisig_rounds_passed;
        if (m_multisig)
        {
          if (!json.HasMember("multisig_signers"))
          {
            LOG_ERROR("Field multisig_signers not found in JSON");
            return false;
          }
          if (!json["multisig_signers"].IsString())
          {
            LOG_ERROR("Field multisig_signers found in JSON, but not String");
            return false;
          }
          const char *field_multisig_signers = json["multisig_signers"].GetString();
          std::string multisig_signers = std::string(field_multisig_signers, field_multisig_signers + json["multisig_signers"].GetStringLength());
          r = ::serialization::parse_binary(multisig_signers, m_multisig_signers);
          if (!r)
          {
            LOG_ERROR("Field multisig_signers found in JSON, but failed to parse");
            return false;
          }

          //previous version of multisig does not have this field
          if (json.HasMember("multisig_derivations"))
          {
            if (!json["multisig_derivations"].IsString())
            {
              LOG_ERROR("Field multisig_derivations found in JSON, but not String");
              return false;
            }
            const char *field_multisig_derivations = json["multisig_derivations"].GetString();
            std::string multisig_derivations = std::string(field_multisig_derivations, field_multisig_derivations + json["multisig_derivations"].GetStringLength());
            r = ::serialization::parse_binary(multisig_derivations, m_multisig_derivations);
            if (!r)
            {
              LOG_ERROR("Field multisig_derivations found in JSON, but failed to parse");
              return false;
            }
          }
        }
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
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_non_default_ring_size, int, Int, false, true);
        m_confirm_non_default_ring_size = field_confirm_non_default_ring_size;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, ask_password, AskPasswordType, Int, false, AskPasswordToDecrypt);
        m_ask_password = field_ask_password;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, default_decimal_point, int, Int, false, CRYPTONOTE_DISPLAY_DECIMAL_POINT);
        // x100, we force decimal point = 10 for existing wallets
        if (field_default_decimal_point != CRYPTONOTE_DISPLAY_DECIMAL_POINT)
          field_default_decimal_point = CRYPTONOTE_DISPLAY_DECIMAL_POINT;

        cryptonote::set_default_decimal_point(field_default_decimal_point);
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, min_output_count, uint32_t, Uint, false, 0);
        m_min_output_count = field_min_output_count;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, min_output_value, uint64_t, Uint64, false, 0);
        m_min_output_value = field_min_output_value;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, merge_destinations, int, Int, false, false);
        m_merge_destinations = field_merge_destinations;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_backlog, int, Int, false, true);
        m_confirm_backlog = field_confirm_backlog;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_backlog_threshold, uint32_t, Uint, false, 0);
        m_confirm_backlog_threshold = field_confirm_backlog_threshold;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, confirm_export_overwrite, int, Int, false, true);
        m_confirm_export_overwrite = field_confirm_export_overwrite;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, auto_low_priority, int, Int, false, true);
        m_auto_low_priority = field_auto_low_priority;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, nettype, uint8_t, Uint, false, static_cast<uint8_t>(m_nettype));
        // The network type given in the program argument is inconsistent with the network type saved in the wallet
        THROW_WALLET_EXCEPTION_IF(static_cast<uint8_t>(m_nettype) != field_nettype, error::wallet_internal_error,
                                  (boost::format("%s wallet cannot be opened as %s wallet")
                                   % (field_nettype == 0 ? "Mainnet" : field_nettype == 1 ? "Testnet" : "Stagenet")
                                   % (m_nettype == cryptonote::MAINNET ? "mainnet" : m_nettype == cryptonote::TESTNET
                                                                         ? "testnet" : "stagenet")).str());
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, segregate_pre_fork_outputs, int, Int, false, true);
        m_segregate_pre_fork_outputs = field_segregate_pre_fork_outputs;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, key_reuse_mitigation2, int, Int, false, true);
        m_key_reuse_mitigation2 = field_key_reuse_mitigation2;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, segregation_height, int, Uint, false, 0);
        m_segregation_height = field_segregation_height;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, ignore_fractional_outputs, int, Int, false, true);
        m_ignore_fractional_outputs = field_ignore_fractional_outputs;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, subaddress_lookahead_major, uint32_t, Uint, false, SUBADDRESS_LOOKAHEAD_MAJOR);
        m_subaddress_lookahead_major = field_subaddress_lookahead_major;
        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, subaddress_lookahead_minor, uint32_t, Uint, false, SUBADDRESS_LOOKAHEAD_MINOR);
        m_subaddress_lookahead_minor = field_subaddress_lookahead_minor;

        GET_FIELD_FROM_JSON_RETURN_ON_ERROR(json, device_name, std::string, String, false, std::string());
        if (m_device_name.empty())
        {
          if (field_device_name_found)
          {
            m_device_name = field_device_name;
          }
          else
          {
            m_device_name = m_key_device_type == hw::device::device_type::LEDGER ? "Ledger" : "default";
          }
        }
    }
    else
    {
      THROW_WALLET_EXCEPTION(error::wallet_internal_error, "invalid password");
      return false;
    }

    r = epee::serialization::load_t_from_binary(m_account, account_data);
    THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);
    if (m_key_device_type == hw::device::device_type::LEDGER) {
      LOG_PRINT_L0("Account on device. Initing device...");
      hw::device &hwdev = hw::get_device(m_device_name);
      hwdev.set_name(m_device_name);
      hwdev.init();
      hwdev.connect();
      m_account.set_device(hwdev);
      LOG_PRINT_L0("Device inited...");
    } else if (key_on_device()) {
      THROW_WALLET_EXCEPTION(error::wallet_internal_error, "hardware device not supported");
    }

    const cryptonote::account_keys& keys = m_account.get_keys();
    hw::device &hwdev = m_account.get_device();
    r = r && hwdev.verify_keys(keys.m_view_secret_key,  keys.m_account_address.m_view_public_key);
    THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);
    if(!m_watch_only && !m_multisig)
      r = r && hwdev.verify_keys(keys.m_spend_secret_key, keys.m_account_address.m_spend_public_key);
    THROW_WALLET_EXCEPTION_IF(!r, error::invalid_password);

    if (r)
      setup_cache_keys(password);

    return true;
}

void tools::GraftWallet::setup_cache_keys(const epee::wipeable_string &password)
{
  crypto::chacha_key key;
  crypto::generate_chacha_key(password.data(), password.size(), key, m_kdf_rounds);

  static_assert(HASH_SIZE == sizeof(crypto::chacha_key), "Mismatched sizes of hash and chacha key");
  epee::mlocked<tools::scrubbed_arr<char, HASH_SIZE+1>> cache_key_data;
  memcpy(cache_key_data.data(), &key, HASH_SIZE);
  cache_key_data[HASH_SIZE] = CACHE_KEY_TAIL;
  cn_fast_hash(cache_key_data.data(), HASH_SIZE+1, (crypto::hash&)m_cache_key);
  get_ringdb_key();
}
