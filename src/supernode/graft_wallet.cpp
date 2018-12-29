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
#include "wallet_errors.h"
#include "api/pending_transaction.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "common/json_util.h"
#include "common/scoped_message_writer.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "readline_buffer.h"
#include "serialization/binary_utils.h"
#include <boost/format.hpp>

using namespace std;
using namespace Monero;

namespace {
static const size_t DEFAULT_MIXIN = 4;
}

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
}

// TODO: this is a method from different API (libwallet_api); try to remove and re-use libwallet api
Monero::PendingTransaction *GraftWallet::createTransaction(const string &dst_addr, const string &payment_id,
                                                           boost::optional<uint64_t> amount, uint32_t mixin_count, const supernode::GraftTxExtra &graftExtra,
                                                           Monero::PendingTransaction::Priority priority)
{
    int status = 0;
    std::string errorString;
    cryptonote::address_parse_info info;

    uint32_t subaddr_account = 0;
    std::set<uint32_t> subaddr_indices;

    // indicates if dst_addr is integrated address (address + payment_id)
    // TODO:  (https://bitcointalk.org/index.php?topic=753252.msg9985441#msg9985441)
    size_t fake_outs_count = mixin_count > 0 ? mixin_count : this->default_mixin();
    if (fake_outs_count == 0)
        fake_outs_count = DEFAULT_MIXIN;

    uint32_t adjusted_priority = this->adjust_priority(static_cast<uint32_t>(priority));

    GraftPendingTransactionImpl * transaction = new GraftPendingTransactionImpl(this);

    do {
        if(!cryptonote::get_account_address_from_str(info, this->nettype(), dst_addr)) {
            errorString = "Invalid destination address";
            break;
        }


        std::vector<uint8_t> extra;
        // if dst_addr is not an integrated address, parse payment_id
        if (!info.has_payment_id && !payment_id.empty()) {
            // copy-pasted from simplewallet.cpp:2212
            crypto::hash payment_id_long;
            bool r = tools::wallet2::parse_long_payment_id(payment_id, payment_id_long);
            if (r) {
                std::string extra_nonce;
                cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_long);
                r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
            } else {
                r = tools::wallet2::parse_short_payment_id(payment_id, info.payment_id);
                if (r) {
                    std::string extra_nonce;
                    cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, info.payment_id);
                    r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
                }
            }

            if (!r) {
                errorString = tr("payment id has invalid format, expected 16 or 64 character hex string: ") + payment_id;
                break;
            }
        }
        else if (info.has_payment_id) {
            std::string extra_nonce;
            cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, info.payment_id);
            bool r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
            if (!r) {
                errorString = tr("Failed to add short payment id: ") + epee::string_tools::pod_to_hex(info.payment_id);
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
                de.addr = info.address;
                de.amount = *amount;
                de.is_subaddress = info.is_subaddress;
                dsts.push_back(de);
                transaction->setPendingTx(this->create_transactions_2(dsts, fake_outs_count, 0 /* unlock_time */,
                                                                            adjusted_priority,
                                                                            extra, subaddr_account, subaddr_indices));
            } else {
                // for the GUI, sweep_all (i.e. amount set as "(all)") will always sweep all the funds in all the addresses
                if (subaddr_indices.empty())
                {
                    for (uint32_t index = 0; index < this->get_num_subaddresses(subaddr_account); ++index)
                        subaddr_indices.insert(index);
                }

                //ak-mr-merge
                const size_t outputs = 0;
                transaction->setPendingTx(this->create_transactions_all(0, info.address, info.is_subaddress, outputs, fake_outs_count,
                                                                        0 /* unlock_time */, adjusted_priority,
                                                                        extra, subaddr_account, subaddr_indices));
            }

        } catch (const tools::error::daemon_busy&) {
            // TODO: make it translatable with "tr"?
            errorString = tr("daemon is busy. Please try again later.");
        } catch (const tools::error::no_connection_to_daemon&) {
            errorString = tr("no connection to daemon. Please make sure daemon is running.");
        } catch (const tools::error::wallet_rpc_error& e) {
            errorString = tr("RPC error: ") +  e.to_string();
        } catch (const tools::error::get_random_outs_error &e) {
            errorString = (boost::format(tr("failed to get random outputs to mix: %s")) % e.what()).str();
        } catch (const tools::error::not_enough_unlocked_money& e) {
            std::ostringstream writer;
            writer << boost::format(tr("not enough money to transfer, available only %s, sent amount %s")) %
                      cryptonote::print_money(e.available()) %
                      cryptonote::print_money(e.tx_amount());
            errorString = writer.str();
        } catch (const tools::error::not_enough_money& e) {
            std::ostringstream writer;
            writer << boost::format(tr("not enough money to transfer, overall balance only %s, sent amount %s")) %
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
                writer << "\n" << tr("output amount") << " = " << cryptonote::print_money(outs_for_amount.first) << ", " << tr("found outputs to use") << " = " << outs_for_amount.second;
            }
            writer << "\n" << tr("Please sweep unmixable outputs.");
            errorString = writer.str();
        } catch (const tools::error::tx_not_constructed&) {
            errorString = tr("transaction was not constructed");
        } catch (const tools::error::tx_rejected& e) {
            std::ostringstream writer;
            writer << (boost::format(tr("transaction %s was rejected by daemon with status: ")) % cryptonote::get_transaction_hash(e.tx())) <<  e.status();
            errorString = writer.str();
        } catch (const tools::error::tx_sum_overflow& e) {
            errorString = e.what();
        } catch (const tools::error::zero_destination&) {
            errorString =  tr("one of destinations is zero");
        } catch (const tools::error::tx_too_big& e) {
            errorString =  tr("failed to find a suitable way to split transactions");
        } catch (const tools::error::transfer_error& e) {
            errorString = string(tr("unknown transfer error: ")) + e.what();
        } catch (const tools::error::wallet_internal_error& e) {
            errorString =  string(tr("internal error: ")) + e.what();
        } catch (const std::exception& e) {
            errorString =  string(tr("unexpected error: ")) + e.what();
        } catch (...) {
            errorString = tr("unknown error");
        }
    } while (false);

    if (!errorString.empty())
    {
        status = Wallet::Status_Error;
    }

    transaction->setStatus(status);
    transaction->setErrorString(errorString);
    return transaction;
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
