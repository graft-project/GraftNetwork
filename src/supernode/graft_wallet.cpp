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
#include "api/pending_transaction.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "common/json_util.h"
#include "common/scoped_message_writer.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "readline_buffer.h"
#include "serialization/binary_utils.h"


using namespace std;

namespace tools {



GraftWallet::GraftWallet(cryptonote::network_type nettype, bool restricted)
    : wallet2(nettype, restricted)
{
}

bool GraftWallet::verify(const std::string &message, const std::string &address, const std::string &signature,  cryptonote::network_type nettype)
{
  cryptonote::address_parse_info info;
  if (!cryptonote::get_account_address_from_str(info, nettype, address)) {
    LOG_ERROR("get_account_address_from_str");
    return false;
  }
  return wallet2::verify(message, info.address, signature);
}


std::unique_ptr<GraftWallet> GraftWallet::createWallet(const string &daemon_address,
                                                       const string &daemon_host, int daemon_port,
                                                       const string &daemon_login, cryptonote::network_type nettype,
                                                       bool restricted)
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
        auto parsed = tools::login::parse(std::move(ldaemon_login), false, [](bool verify) {
            #ifdef HAVE_READLINE
            rdln::suspend_readline pause_readline;
            #endif
           return tools::password_container::prompt(verify, "Daemon client password");
        });

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
        daemon_port = nettype == cryptonote::MAINNET ? config::RPC_DEFAULT_PORT
                                                     : nettype == cryptonote::STAGENET ? config::stagenet::RPC_DEFAULT_PORT
                                                                                       : config::testnet::RPC_DEFAULT_PORT;
    }
    std::string ldaemon_address = daemon_address;
    if (daemon_address.empty())
    {
        ldaemon_address = std::string("http://") + ldaemon_host + ":" + std::to_string(daemon_port);
    }
    std::unique_ptr<tools::GraftWallet> wallet(new tools::GraftWallet(nettype, restricted));
    wallet->init(std::move(ldaemon_address), std::move(login));
    return wallet;
}

std::unique_ptr<GraftWallet> GraftWallet::createWallet(const string &account_data, const string &password,
                                                       const string &daemon_address, const string &daemon_host,
                                                       int daemon_port, const string &daemon_login,
                                                       cryptonote::network_type nettype, bool restricted)
{
    auto wallet = createWallet(daemon_address, daemon_host, daemon_port, daemon_login,
                               nettype, restricted);
    if (wallet)
    {
        wallet->load_graft(account_data, password, "" /*cache_file*/);
    }
    return std::move(wallet);
}

crypto::secret_key GraftWallet::generate_graft(const string &password, const crypto::secret_key &recovery_param, bool recover, bool two_random)
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
        std::string err;
        uint64_t height = 0;

        // we get the max of approximated height and known height
        // approximated height is the least of daemon target height
        // (the max of what the other daemons are claiming is their
        // height) and the theoretical height based on the local
        // clock. This will be wrong only if both the local clock
        // is bad *and* a peer daemon claims a highest height than
        // the real chain.
        // known height is the height the local daemon is currently
        // synced to, it will be lower than the real chain height if
        // the daemon is currently syncing.
        height = get_approximate_blockchain_height();
        uint64_t target_height = get_daemon_blockchain_target_height(err);
        if (err.empty() && target_height < height)
            height = target_height;
        uint64_t local_height = get_daemon_blockchain_height(err);
        if (err.empty() && local_height > height)
            height = local_height;
        m_refresh_from_block_height = height >= blocks_per_month ? height - blocks_per_month : 0;
    }

    ///bool r = store_keys(m_keys_file, password, false);
    ///THROW_WALLET_EXCEPTION_IF(!r, error::file_save_error, m_keys_file);

    cryptonote::block b;
    generate_genesis(b);
    m_blockchain.push_back(get_block_hash(b));

    ///store();
    return retval;
}

void GraftWallet::load_graft(const string &data, const string &password, const std::string &cache_file)
{
    clear();

    if (!load_keys_graft(data, password))
    {
      THROW_WALLET_EXCEPTION_IF(true, error::file_read_error, m_keys_file);
    }
    LOG_PRINT_L0("Loaded wallet keys file, with public address: " << m_account.get_public_address_str(m_nettype));

    //keys loaded ok!
    //try to load wallet file. but even if we failed, it is not big problem

    m_account_public_address = m_account.get_keys().m_account_address;

    if (!cache_file.empty())
      load_cache(cache_file);

    cryptonote::block genesis;
    generate_genesis(genesis);
    crypto::hash genesis_hash = cryptonote::get_block_hash(genesis);

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
//----------------------------------------------------------------------------------------------------
void GraftWallet::load_cache(const std::string &filename)
{
    wallet2::cache_file_data cache_file_data;
    std::string buf;
    bool r = epee::file_io_utils::load_file_to_string(filename, buf);
    THROW_WALLET_EXCEPTION_IF(!r, error::file_read_error, filename);
    // try to read it as an encrypted cache
    try
    {
        LOG_PRINT_L1("Trying to decrypt cache data");
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
        LOG_PRINT_L1("Failed to load encrypted cache, trying unencrypted");
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

//----------------------------------------------------------------------------------------------------
void GraftWallet::store_cache(const string &filename)
{
    // preparing wallet data
    std::stringstream oss;
    boost::archive::portable_binary_oarchive ar(oss);
    ar << *this;

    GraftWallet::cache_file_data cache_file_data = boost::value_initialized<GraftWallet::cache_file_data>();
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

Monero::PendingTransaction *GraftWallet::createTransaction(const string &dst_addr, const string &payment_id, boost::optional<uint64_t> amount,
                                                           uint32_t mixin_count, const supernode::GraftTxExtra &graftExtra,
                                                           Monero::PendingTransaction::Priority priority)
{

}



std::string GraftWallet::store_keys_graft(const std::string& password, bool watch_only)
{
  std::string result;
  if (wallet2::store_keys_to_buffer(epee::wipeable_string(password), result, watch_only))
    return result;
  else
    return "";
}

bool GraftWallet::load_keys_graft(const string &data, const string &password)
{
  return wallet2::load_keys_from_buffer(data, epee::wipeable_string(password));
}


} // namespace tools
