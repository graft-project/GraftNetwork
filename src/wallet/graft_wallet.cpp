#include "graft_wallet.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "api/graft_pending_transaction.h"
#include "common/scoped_message_writer.h"
#include "serialization/binary_utils.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "common/json_util.h"
#include "string_coding.h"
#include <boost/format.hpp>

static const size_t DEFAULT_MIXIN = 4;

namespace
{
  bool verify_keys(const crypto::secret_key& sec, const crypto::public_key& expected_pub)
  {
    crypto::public_key pub;
    bool r = crypto::secret_key_to_public_key(sec, pub);
    return r && expected_pub == pub;
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
tools::GraftWallet::GraftWallet(bool testnet, bool restricted)
    : wallet2(testnet, restricted)
{
}

std::unique_ptr<tools::GraftWallet> tools::GraftWallet::createWallet(const string &daemon_address,
                     const string &daemon_host, int daemon_port, const string &daemon_login,
                     bool testnet, bool restricted)
{
    //make_basic() analogue
    if (!daemon_address.empty() && !daemon_host.empty() && 0 != daemon_port)
    {
        tools::fail_msg_writer() << tools::GraftWallet::tr("can't specify daemon host or port more than once");
        return nullptr;
    }
    boost::optional<epee::net_utils::http::login> login{};
    if (!daemon_login.empty())
    {
        std::string ldaemon_login(daemon_login);
        auto parsed = tools::login::parse(std::move(ldaemon_login), false, "Daemon client password");
        if (!parsed)
        {
            return nullptr;
        }
        login.emplace(std::move(parsed->username), std::move(parsed->password).password());
    }
    std::string ldaemon_host = daemon_host;
    if (daemon_host.empty())
    {
        ldaemon_host = "localhost";
    }
    if (!daemon_port)
    {
        daemon_port = testnet ? config::testnet::RPC_DEFAULT_PORT : config::RPC_DEFAULT_PORT;
    }
    std::string ldaemon_address = daemon_address;
    if (daemon_address.empty())
    {
        ldaemon_address = std::string("http://") + ldaemon_host + ":" + std::to_string(daemon_port);
    }
    std::unique_ptr<tools::GraftWallet> wallet(new tools::GraftWallet(testnet, restricted));
    wallet->init(std::move(ldaemon_address), std::move(login));
    return wallet;
}

std::unique_ptr<tools::GraftWallet> tools::GraftWallet::createWallet(const string &account_data,
                     const string &password, const string &daemon_address,
                     const string &daemon_host, int daemon_port, const string &daemon_login,
                     bool testnet, bool use_base64, bool restricted)
{
    auto wallet = createWallet(daemon_address, daemon_host, daemon_port, daemon_login,
                               testnet, restricted);
    if (wallet)
    {
        wallet->loadFromData(account_data, password, "" /*cache_file*/, use_base64);
    }
    return std::move(wallet);
}

crypto::secret_key tools::GraftWallet::generateFromData(const std::string &password,
                                                         const crypto::secret_key &recovery_param,
                                                         bool recover, bool two_random)
{
    clear();

    crypto::secret_key retval = m_account.generate(recovery_param, recover, two_random);

    m_account_public_address = m_account.get_keys().m_account_address;
    m_watch_only = false;

    // -1 month for fluctuations in block time and machine date/time setup.
    // avg seconds per block
    const int seconds_per_block = DIFFICULTY_TARGET_V2;
    // ~num blocks per month
    const uint64_t blocks_per_month = 60*60*24*30/seconds_per_block;

    // try asking the daemon first
    if(m_refresh_from_block_height == 0 && !recover){
      uint64_t height = estimate_blockchain_height();
      m_refresh_from_block_height = height >= blocks_per_month ? height - blocks_per_month : 0;
    }

    cryptonote::block b;
    generate_genesis(b);
    m_blockchain.push_back(get_block_hash(b));

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
    LOG_PRINT_L0("Loaded wallet keys file, with public address: " << m_account.get_public_address_str(m_testnet));

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
    }
    else
    {
        check_genesis(genesis_hash);
    }

    m_local_bc_height = m_blockchain.size();
}

void tools::GraftWallet::load_cache(const std::string &filename)
{
    wallet2::cache_file_data cache_file_data;
    std::string buf;
    bool r = epee::file_io_utils::load_file_to_string(filename, buf);
    THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, filename);
    // try to read it as an encrypted cache
    try
    {
        LOG_PRINT_L0("Trying to decrypt cache data");
        r = ::serialization::parse_binary(buf, cache_file_data);
        THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "internal error: failed to deserialize \"" + filename + '\"');
        crypto::chacha_key key;
        generate_chacha_key_from_secret_keys(key);
        std::string cache_data;
        cache_data.resize(cache_file_data.cache_data.size());
        crypto::chacha8(cache_file_data.cache_data.data(), cache_file_data.cache_data.size(), key, cache_file_data.iv, &cache_data[0]);

        std::stringstream iss;
        iss << cache_data;
        try {
            boost::archive::portable_binary_iarchive ar(iss);
            ar >> *this;
        }
        catch (...)
        {
            LOG_PRINT_L0("Failed to open portable binary, trying unportable");
            boost::filesystem::copy_file(filename, filename + ".unportable", boost::filesystem::copy_option::overwrite_if_exists);
            iss.str("");
            iss << cache_data;
            boost::archive::binary_iarchive ar(iss);
            ar >> *this;
        }
    }
    catch (...)
    {
        LOG_PRINT_L0("Failed to load encrypted cache, trying unencrypted");
        std::stringstream iss;
        iss << buf;
        try {
            boost::archive::portable_binary_iarchive ar(iss);
            ar >> *this;
        }
        catch (...)
        {
            LOG_PRINT_L0("Failed to open portable binary, trying unportable");
            boost::filesystem::copy_file(filename, filename + ".unportable", boost::filesystem::copy_option::overwrite_if_exists);
            iss.str("");
            iss << buf;
            boost::archive::binary_iarchive ar(iss);
            ar >> *this;
        }
    }
    THROW_WALLET_EXCEPTION_IF(
                m_account_public_address.m_spend_public_key != m_account.get_keys().m_account_address.m_spend_public_key ||
            m_account_public_address.m_view_public_key  != m_account.get_keys().m_account_address.m_view_public_key,
                error::wallet_files_doesnt_correspond, m_keys_file, filename);
}

void tools::GraftWallet::store_cache(const std::string &filename)
{
    // preparing wallet data
    std::stringstream oss;
    boost::archive::portable_binary_oarchive ar(oss);
    ar << *static_cast<tools::wallet2*>(this);

    wallet2::cache_file_data cache_file_data = boost::value_initialized<wallet2::cache_file_data>();
    cache_file_data.cache_data = oss.str();
    crypto::chacha_key key;
    generate_chacha_key_from_secret_keys(key);
    std::string cipher;
    cipher.resize(cache_file_data.cache_data.size());
    cache_file_data.iv = crypto::rand<crypto::chacha_iv>();
    crypto::chacha8(cache_file_data.cache_data.data(), cache_file_data.cache_data.size(), key, cache_file_data.iv, &cipher[0]);
    cache_file_data.cache_data = cipher;

    // save to new file
    std::ofstream ostr;
    ostr.open(filename, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    binary_archive<true> oar(ostr);
    bool success = ::serialization::serialize(oar, cache_file_data);
    ostr.close();
    THROW_WALLET_EXCEPTION_IF(!success || !ostr.good(), error::file_save_error, filename);
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
    crypto::hash payment_id = cryptonote::null_hash;
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
    add_unconfirmed_tx(ptx.tx, amount_in, dests, payment_id, ptx.change_dts.amount);
    if (store_tx_info())
    {
        LOG_PRINT_L2("storing tx key " << ptx.tx_key);
        m_tx_keys.insert(std::make_pair(txid, ptx.tx_key));
        LOG_PRINT_L2("there're  " << m_tx_keys.size() << " stored keys");
    }

    LOG_PRINT_L2("transaction " << txid << " generated ok and sent to daemon, key_images: [" << ptx.key_images << "]");

    for(size_t idx: ptx.selected_transfers)
    {
        set_spent(idx, 0);
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

    wallet2::keys_file_data keys_file_data = boost::value_initialized<wallet2::keys_file_data>();

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

bool tools::GraftWallet::load_keys_from_data(const std::string &data, const std::string &password)
{
    std::string account_data;
    if (false)
    {
        wallet2::keys_file_data keys_file_data;
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

Monero::PendingTransaction *tools::GraftWallet::createTransaction(const string &dst_addr,
                  const string &payment_id, boost::optional<uint64_t> amount, uint32_t mixin_count,
                  const supernode::GraftTxExtra &graftExtra,
                  Monero::PendingTransaction::Priority priority)
{
    int status = 0;
    std::string errorString;
    cryptonote::account_public_address addr;

    // indicates if dst_addr is integrated address (address + payment_id)
    bool has_payment_id;
    crypto::hash8 payment_id_short;
    // TODO:  (https://bitcointalk.org/index.php?topic=753252.msg9985441#msg9985441)
    size_t fake_outs_count = mixin_count > 0 ? mixin_count : this->default_mixin();
    if (fake_outs_count == 0)
        fake_outs_count = DEFAULT_MIXIN;

    Monero::GraftPendingTransactionImpl *transaction = new Monero::GraftPendingTransactionImpl(this);

    do {
        if(!cryptonote::get_account_integrated_address_from_str(addr, has_payment_id, payment_id_short, this->testnet(), dst_addr)) {
            // TODO: copy-paste 'if treating as an address fails, try as url' from simplewallet.cpp:1982
            errorString = "Invalid destination address";
            break;
        }

        std::vector<uint8_t> extra;
        // if dst_addr is not an integrated address, parse payment_id
        if (!has_payment_id && !payment_id.empty()) {
            // copy-pasted from simplewallet.cpp:2212
            crypto::hash payment_id_long;
            bool r = tools::GraftWallet::parse_long_payment_id(payment_id, payment_id_long);
            if (r) {
                std::string extra_nonce;
                cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_long);
                r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
            } else {
                r = tools::GraftWallet::parse_short_payment_id(payment_id, payment_id_short);
                if (r) {
                    std::string extra_nonce;
                    cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_short);
                    r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
                }
            }

            if (!r) {
                errorString = std::string("payment id has invalid format, expected 16 or 64 character hex string: ") + payment_id;
                break;
            }
        }
        else if (has_payment_id) {
            std::string extra_nonce;
            cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_short);
            bool r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
            if (!r) {
                errorString = std::string("Failed to add short payment id: ") + epee::string_tools::pod_to_hex(payment_id_short);
                break;
            }
        }

        // add graft extra fields to tx extra
        if (!cryptonote::add_graft_tx_extra_to_extra(extra, graftExtra)) {
            LOG_ERROR("Error adding graft fields to tx extra");
            delete transaction;
            return nullptr;
        }


        try {
            if (amount) {
                vector<cryptonote::tx_destination_entry> dsts;
                cryptonote::tx_destination_entry de;
                de.addr = addr;
                de.amount = *amount;
                dsts.push_back(de);
                transaction->setPendingTx(create_transactions_2(dsts, fake_outs_count, 0 /* unlock_time */,
                                                                static_cast<uint32_t>(priority),
                                                                extra, false));
            } else {
                transaction->setPendingTx(create_transactions_all(0, addr, fake_outs_count, 0 /* unlock_time */,
                                                                  static_cast<uint32_t>(priority),
                                                                  extra, false));
            }

        } catch (const tools::error::daemon_busy&) {
            // TODO: make it translatable with "tr"?
            errorString = "daemon is busy. Please try again later.";
        } catch (const tools::error::no_connection_to_daemon&) {
            errorString = "no connection to daemon. Please make sure daemon is running.";
        } catch (const tools::error::wallet_rpc_error& e) {
            errorString = tr("RPC error: ") +  e.to_string();
        } catch (const tools::error::get_random_outs_error &e) {
            errorString = (boost::format(tr("failed to get random outputs to mix: %s")) % e.what()).str();

        } catch (const tools::error::not_enough_money& e) {
            std::ostringstream writer;

            writer << boost::format(tr("not enough money to transfer, available only %s, sent amount %s")) %
                      cryptonote::print_money(e.available()) %
                      cryptonote::print_money(e.tx_amount());
            errorString = writer.str();

        } catch (const tools::error::tx_not_possible& e) {
            std::ostringstream writer;

            writer << boost::format(tr("not enough money to transfer, available only %s, transaction amount %s = %s + %s (fee)")) %
                      cryptonote::print_money(e.available()) %
                      cryptonote::print_money(e.tx_amount() + e.fee())  %
                      cryptonote::print_money(e.tx_amount()) %
                      cryptonote::print_money(e.fee());
            errorString = writer.str();

        } catch (const tools::error::not_enough_outs_to_mix& e) {
            std::ostringstream writer;
            writer << tr("not enough outputs for specified ring size") << " = " << (e.mixin_count() + 1) << ":";
            for (const std::pair<uint64_t, uint64_t> outs_for_amount : e.scanty_outs()) {
                writer << "\n" << tr("output amount") << " = " << cryptonote::print_money(outs_for_amount.first)
                       << ", " << tr("found outputs to use") << " = " << outs_for_amount.second;
            }
            errorString = writer.str();
        } catch (const tools::error::tx_not_constructed&) {
            errorString = tr("transaction was not constructed");
        } catch (const tools::error::tx_rejected& e) {
            std::ostringstream writer;
            writer << (boost::format(tr("transaction %s was rejected by daemon with status: ")) % get_transaction_hash(e.tx())) <<  e.status();
            errorString = writer.str();
        } catch (const tools::error::tx_sum_overflow& e) {
            errorString = e.what();
        } catch (const tools::error::zero_destination&) {
            errorString = "one of destinations is zero";
        } catch (const tools::error::tx_too_big& e) {
            errorString = "failed to find a suitable way to split transactions";
        } catch (const tools::error::transfer_error& e) {
            errorString = string("unknown transfer error: ") + e.what();
        } catch (const tools::error::wallet_internal_error& e) {
            errorString =  string("internal error: ") + e.what();
        } catch (const std::exception& e) {
            errorString =  string("unexpected error: ") + e.what();
        } catch (...) {
            errorString = "unknown error";
        }
    }
    while (false);
    if (!errorString.empty())
    {
        status = Monero::Wallet::Status_Error;
    }

    transaction->setStatus(status);
    transaction->setErrorString(errorString);
    return transaction;
}

bool tools::GraftWallet::verifySignedMessage(const string &message, const string &address, const string &signature, bool isTestnet)
{
    cryptonote::account_public_address addr;
    bool has_payment_id;
    crypto::hash8 payment_id;

    if (!cryptonote::get_account_integrated_address_from_str(addr, has_payment_id, payment_id, isTestnet, address)) {
        LOG_PRINT_L0("!get_account_integrated_address_from_str");
        return false;
    }

    return verify(message, addr, signature);
}
