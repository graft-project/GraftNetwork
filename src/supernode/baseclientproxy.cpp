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

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "mnemonics/electrum-words.h"
#include "common/command_line.h"
#include "baseclientproxy.h"
#include "graft_defines.h"
#include "string_coding.h"
#include "common/util.h"

static const std::string scWalletCachePath("/cache/");
static const size_t DEFAULT_MIXIN = 4;

supernode::BaseClientProxy::BaseClientProxy()
{
}

void supernode::BaseClientProxy::Init()
{
    m_DAPIServer->ADD_DAPI_HANDLER(GetWalletBalance, rpc_command::GET_WALLET_BALANCE, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(CreateAccount, rpc_command::CREATE_ACCOUNT, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(GetSeed, rpc_command::GET_SEED, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(RestoreAccount, rpc_command::RESTORE_ACCOUNT, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(Transfer, rpc_command::TRANSFER, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(GetTransferFee, rpc_command::GET_TRANSFER_FEE, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(GetRawTransaction, rpc_command::GET_RAW_TRANSACTION, BaseClientProxy);
}

bool supernode::BaseClientProxy::GetWalletBalance(const supernode::rpc_command::GET_WALLET_BALANCE::request &in, supernode::rpc_command::GET_WALLET_BALANCE::response &out)
{
	LOG_PRINT_L0("BaseClientProxy::GetWalletBalance" << in.Account);
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(in.Account), in.Password);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    try
    {
        wal->refresh();
        out.Balance = wal->balance();
        out.UnlockedBalance = wal->unlocked_balance();
        storeWalletState(wal.get());
    }
    catch (const std::exception& e)
    {
        out.Result = ERROR_BALANCE_NOT_AVAILABLE;
        return false;
    }
    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::CreateAccount(const supernode::rpc_command::CREATE_ACCOUNT::request &in, supernode::rpc_command::CREATE_ACCOUNT::response &out)
{
	LOG_PRINT_L0("BaseClientProxy::CreateAccount" << in.Language);
    std::vector<std::string> languages;
    crypto::ElectrumWords::get_language_list(languages);
    std::vector<std::string>::iterator it;
    it = std::find(languages.begin(), languages.end(), in.Language);
    if (it == languages.end())
    {
        out.Result = ERROR_LANGUAGE_IS_NOT_FOUND;
        return false;
    }

    std::unique_ptr<tools::GraftWallet> wal =
            tools::GraftWallet::createWallet(std::string(), m_Servant->GetNodeIp(), m_Servant->GetNodePort(),
                                             m_Servant->GetNodeLogin(), m_Servant->IsTestnet());
    if (!wal)
    {
        out.Result = ERROR_CREATE_WALLET_FAILED;
        return false;
    }
    wal->set_seed_language(in.Language);
    wal->set_refresh_from_block_height(m_Servant->GetCurrentBlockHeight());
    crypto::secret_key dummy_key;
    try
    {
        wal->generateFromData(in.Password, dummy_key, false, false);
    }
    catch (const std::exception& e)
    {
        out.Result = ERROR_CREATE_WALLET_FAILED;
        return false;
    }
    out.Account = base64_encode(wal->store_keys_to_data(in.Password));
    out.Address = wal->get_account().get_public_address_str(wal->testnet());
    out.ViewKey = epee::string_tools::pod_to_hex(wal->get_account().get_keys().m_view_secret_key);
    std::string seed;
    wal->get_seed(seed);
    out.Seed = seed;
    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::GetSeed(const supernode::rpc_command::GET_SEED::request &in, supernode::rpc_command::GET_SEED::response &out)
{
	LOG_PRINT_L0("BaseClientProxy::GetSeed" << in.Account);
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(in.Account), in.Password);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    wal->set_seed_language(in.Language);
    std::string seed;
    wal->get_seed(seed);
    out.Seed = seed;
    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::RestoreAccount(const supernode::rpc_command::RESTORE_ACCOUNT::request &in, supernode::rpc_command::RESTORE_ACCOUNT::response &out)
{
	LOG_PRINT_L0("BaseClientProxy::RestoreAccount" << in.Seed);
    if (in.Seed.empty())
    {
        out.Result = ERROR_ELECTRUM_SEED_EMPTY;
        return false;
    }
    crypto::secret_key recovery_key;
    std::string old_language;
    if (!crypto::ElectrumWords::words_to_bytes(in.Seed, recovery_key, old_language))
    {
        out.Result = ERROR_ELECTRUM_SEED_INVALID;
        return false;
    }
    std::unique_ptr<tools::GraftWallet> wal =
        tools::GraftWallet::createWallet(std::string(), m_Servant->GetNodeIp(), m_Servant->GetNodePort(),
                                             m_Servant->GetNodeLogin(), m_Servant->IsTestnet());
    if (!wal)
    {
        out.Result = ERROR_CREATE_WALLET_FAILED;
        return false;
    }
    try
    {
        wal->set_seed_language(old_language);
        wal->generateFromData(in.Password, recovery_key, true, false);
        out.Account = base64_encode(wal->store_keys_to_data(in.Password));
        out.Address = wal->get_account().get_public_address_str(wal->testnet());
        out.ViewKey = epee::string_tools::pod_to_hex(
                    wal->get_account().get_keys().m_view_secret_key);
        std::string seed;
        wal->get_seed(seed);
        out.Seed = seed;
    }
    catch (const std::exception &e)
    {
        out.Result = ERROR_RESTORE_WALLET_FAILED;
        return false;
    }
    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::GetTransferFee(const supernode::rpc_command::GET_TRANSFER_FEE::request &in, supernode::rpc_command::GET_TRANSFER_FEE::response &out)
{
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(in.Account), in.Password);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    if (wal->restricted())
    {
        //      er.code = WALLET_RPC_ERROR_CODE_DENIED;
        //      er.message = "Command unavailable in restricted mode.";
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    std::vector<cryptonote::tx_destination_entry> dsts;
    std::vector<uint8_t> extra;

    std::string payment_id = "";

    uint64_t amount;
    std::istringstream iss(in.Amount);
    iss >> amount;

    // validate the transfer requested and populate dsts & extra
    if (!validate_transfer(in.Account, in.Password, in.Address, amount, payment_id, dsts, extra))
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    try
    {
        uint64_t mixin = 4;
        uint64_t unlock_time = 0;
        uint64_t priority = 0;
        std::vector<tools::GraftWallet::pending_tx> ptx_vector =
                wal->create_transactions_2(dsts, mixin, unlock_time, priority, extra, false);

        // reject proposed transactions if there are more than one.  see on_transfer_split below.
        if (ptx_vector.size() != 1)
        {
            //        er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
            //        er.message = "Transaction would be too large.  try /transfer_split.";
            out.Result = ERROR_OPEN_WALLET_FAILED;
            return false;
        }

        out.Fee = ptx_vector.back().fee;
    }
    catch (const tools::error::daemon_busy& e)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY;
        //      er.message = e.what();
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    catch (const std::exception& e)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
        //      er.message = e.what();
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    catch (...)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
        //      er.message = "WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR";
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::Transfer(const supernode::rpc_command::TRANSFER::request &in, supernode::rpc_command::TRANSFER::response &out)
{
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(in.Account), in.Password);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    if (wal->restricted())
    {
        //      er.code = WALLET_RPC_ERROR_CODE_DENIED;
        //      er.message = "Command unavailable in restricted mode.";
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    std::vector<cryptonote::tx_destination_entry> dsts;
    std::vector<uint8_t> extra;

    std::string payment_id = "";

    uint64_t amount;
    std::istringstream iss(in.Amount);
    iss >> amount;

    // validate the transfer requested and populate dsts & extra
    if (!validate_transfer(in.Account, in.Password, in.Address, amount, payment_id, dsts, extra))
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    try
    {
        uint64_t mixin = 4;
        uint64_t unlock_time = 0;
        uint64_t priority = 0;
        bool do_not_relay = false;
        bool get_tx_key = false;
        bool get_tx_hex = false;
        std::vector<tools::GraftWallet::pending_tx> ptx_vector =
                wal->create_transactions_2(dsts, mixin, unlock_time, priority, extra, false);

        // reject proposed transactions if there are more than one.  see on_transfer_split below.
        if (ptx_vector.size() != 1)
        {
            //        er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
            //        er.message = "Transaction would be too large.  try /transfer_split.";
            out.Result = ERROR_OPEN_WALLET_FAILED;
            return false;
        }

        if (!do_not_relay)
        {
            wal->commit_tx(ptx_vector);
            storeWalletState(wal.get());
        }

        // populate response with tx hash
        //        std::string tx_hash = epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(ptx_vector.back().tx));
        //        if (get_tx_key)
        //        {
        //            std::string tx_key = epee::string_tools::pod_to_hex(ptx_vector.back().tx_key);
        //        }
        //        uint64_t fee = ptx_vector.back().fee;

        //        if (get_tx_hex)
        //        {
        //            cryptonote::blobdata blob;
        //            tx_to_blob(ptx_vector.back().tx, blob);
        //            res.tx_blob = epee::string_tools::buff_to_hex_nodelimer(blob);
        //        }
    }
    catch (const tools::error::daemon_busy& e)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY;
        //      er.message = e.what();
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    catch (const std::exception& e)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
        //      er.message = e.what();
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    catch (...)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
        //      er.message = "WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR";
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::GetRawTransaction(const supernode::rpc_command::GET_RAW_TRANSACTION::request &in, supernode::rpc_command::GET_RAW_TRANSACTION::response &out)
{
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(in.Account), in.Password);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    if (wal->restricted())
    {
        //      er.code = WALLET_RPC_ERROR_CODE_DENIED;
        //      er.message = "Command unavailable in restricted mode.";
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    size_t fake_outs_count = wal->default_mixin();
    if (fake_outs_count == 0)
        fake_outs_count = DEFAULT_MIXIN;

    vector<cryptonote::tx_destination_entry> dsts;
    std::vector<uint8_t> extra;

    for (auto dest : in.Destinations)
    {
        cryptonote::account_public_address addr;

        // indicates if dst_addr is integrated address (address + payment_id)
        bool has_payment_id;
        crypto::hash8 payment_id_short;
        if (!cryptonote::get_account_integrated_address_from_str(addr, has_payment_id,
                                                                 payment_id_short,
                                                                 wal->testnet(), dest.Address))
        {
            // TODO: copy-paste 'if treating as an address fails, try as url' from simplewallet.cpp:1982
            out.Result = ERROR_INVALID_DESTINATION_ADDRESS;
            return false;
        }

        // if dst_addr is not an integrated address, parse payment_id
        if (!has_payment_id && !dest.PaymentID.empty())
        {
            // copy-pasted from simplewallet.cpp:2212
            crypto::hash payment_id_long;
            bool r = tools::GraftWallet::parse_long_payment_id(dest.PaymentID, payment_id_long);
            if (r) {
                std::string extra_nonce;
                cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_long);
                r = add_extra_nonce_to_tx_extra(extra, extra_nonce);
            } else {
                r = tools::GraftWallet::parse_short_payment_id(dest.PaymentID, payment_id_short);
                if (r) {
                    std::string extra_nonce;
                    cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_short);
                    r = add_extra_nonce_to_tx_extra(extra, extra_nonce);
                }
            }

            if (!r) {
                out.Result = ERROR_INVALID_PAYMENT;
                return false;
            }
        } else if (has_payment_id) {
            std::string extra_nonce;
            cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_short);
            bool r = add_extra_nonce_to_tx_extra(extra, extra_nonce);
            if (!r) {
//                m_errorString = tr("Failed to add short payment id: ") + epee::string_tools::pod_to_hex(payment_id_short);
                out.Result = ERROR_INVALID_PAYMENT;
                return false;
            }
        }

        uint64_t amount;
        std::istringstream iss(dest.Amount);
        iss >> amount;

        cryptonote::tx_destination_entry de;
        de.addr = addr;
        de.amount = amount;
        dsts.push_back(de);
    }

    try {
        uint32_t priority = 1; //Monero::PendingTransaction::Priority_Low
        std::vector<tools::GraftWallet::pending_tx> pending_tx;
        pending_tx = wal->create_transactions_2(dsts, fake_outs_count, 0 /* unlock_time */,
                                                priority, extra, true, in.EnableRta);
        std::vector<std::string> txs;
        uint64_t totalFee = 0;
        for (auto rit = pending_tx.rbegin(); rit != pending_tx.rend(); ++rit)
        {
            tools::GraftWallet::pending_tx ptx = *rit;
            txs.push_back(epee::string_tools::buff_to_hex_nodelimer(cryptonote::tx_to_blob(ptx.tx)));
            totalFee += ptx.fee;

            wal->update_tx_cache(ptx);
        }
        out.Transactions = txs;

        std::stringstream feeStream;
        feeStream << totalFee;
        out.Fee = feeStream.str();
    } catch (const tools::error::daemon_busy&) {
//        m_errorString = tr("daemon is busy. Please try again later.");
        out.Result = ERROR_DAEMON_BUSY;
    } catch (const tools::error::no_connection_to_daemon&) {
//        m_errorString = tr("no connection to daemon. Please make sure daemon is running.");
        out.Result = ERROR_NO_DAEMON_CONNECTION;
    } catch (const tools::error::wallet_rpc_error& e) {
//        m_errorString = tr("RPC error: ") +  e.to_string();
        out.Result = ERROR_DAEMON_RPC_ERROR;
    } catch (const tools::error::get_random_outs_error &e) {
//        m_errorString = (boost::format(tr("failed to get random outputs to mix: %s")) % e.what()).str();
        out.Result = ERROR_GET_RANDOM_OUTPUTS_FAILED;
    } catch (const tools::error::not_enough_money& e) {
//        std::ostringstream writer;
//        writer << boost::format(tr("not enough money to transfer, available only %s, sent amount %s")) %
//                  print_money(e.available()) %
//                  print_money(e.tx_amount());
//        m_errorString = writer.str();
        out.Result = ERROR_NOT_ENOUGH_MONEY;
    } catch (const tools::error::tx_not_possible& e) {
//        std::ostringstream writer;
//        writer << boost::format(tr("not enough money to transfer, available only %s, transaction amount %s = %s + %s (fee)")) %
//                  print_money(e.available()) %
//                  print_money(e.tx_amount() + e.fee())  %
//                  print_money(e.tx_amount()) %
//                  print_money(e.fee());
//        m_errorString = writer.str();
        out.Result = ERROR_NOT_ENOUGH_MONEY;
    } catch (const tools::error::not_enough_outs_to_mix& e) {
//        std::ostringstream writer;
//        writer << tr("not enough outputs for specified ring size") << " = " << (e.mixin_count() + 1) << ":";
//        for (const std::pair<uint64_t, uint64_t> outs_for_amount : e.scanty_outs()) {
//            writer << "\n" << tr("output amount") << " = " << print_money(outs_for_amount.first) << ", " << tr("found outputs to use") << " = " << outs_for_amount.second;
//        }
//        m_errorString = writer.str();
        out.Result = ERROR_NOT_ENOUGH_OUTPUTS;
    } catch (const tools::error::tx_not_constructed&) {
//        m_errorString = tr("transaction was not constructed");
        out.Result = ERROR_TRANSACTION_NOT_CONSTRUCTED;
    } catch (const tools::error::tx_rejected& e) {
//        std::ostringstream writer;
//        writer << (boost::format(tr("transaction %s was rejected by daemon with status: ")) % get_transaction_hash(e.tx())) <<  e.status();
//        m_errorString = writer.str();
        out.Result = ERROR_TRANSACTION_REJECTED;
    } catch (const tools::error::tx_sum_overflow& e) {
//        m_errorString = e.what();
        out.Result = ERROR_TRANSACTION_SUM_OVERFLOW;
    } catch (const tools::error::zero_destination&) {
//        m_errorString =  tr("one of destinations is zero");
        out.Result = ERROR_ZERO_DESTINATION;
    } catch (const tools::error::tx_too_big& e) {
//        m_errorString =  tr("failed to find a suitable way to split transactions");
        out.Result = ERROR_TRANSACTION_TOO_BIG;
    } catch (const tools::error::transfer_error& e) {
//        m_errorString = string(tr("unknown transfer error: ")) + e.what();
        out.Result = ERROR_TRANSFER_ERROR;
    } catch (const tools::error::wallet_internal_error& e) {
//        m_errorString =  string(tr("internal error: ")) + e.what();
        out.Result = ERROR_WALLET_INTERNAL_ERROR;
    } catch (const std::exception& e) {
//        m_errorString =  string(tr("unexpected error: ")) + e.what();
        out.Result = ERROR_UNEXPECTED_ERROR;
    } catch (...) {
//        m_errorString = tr("unknown error");
        out.Result = ERROR_UNKNOWN_ERROR;
    }

    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::validate_transfer(const string &account, const string &password,
                                                   const string &address, uint64_t amount,
                                                   const string payment_id,
                                                   std::vector<cryptonote::tx_destination_entry> &dsts,
                                                   std::vector<uint8_t> &extra)
{
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(account), password);
    if (!wal)
    {
        return false;
    }

    crypto::hash8 integrated_payment_id = cryptonote::null_hash8;
    std::string extra_nonce;

    cryptonote::tx_destination_entry de;
    bool has_payment_id;
    crypto::hash8 new_payment_id;
    std::string errorString;
    if (!get_account_address_from_str_or_url(de.addr, has_payment_id, new_payment_id, wal->testnet(),address,
                                             [&errorString](const std::string &url, const std::vector<std::string> &addresses, bool dnssec_valid)->std::string {
                                             if (!dnssec_valid)
                                             {
                                                 errorString = std::string("Invalid DNSSEC for ") + url;
                                                 return {};
                                             }
                                             if (addresses.empty())
                                             {
                                                 errorString = std::string("No Monero address found at ") + url;
                                                 return {};
                                             }
                                             return addresses[0];
                                    }))
    {
//        er.code = WALLET_RPC_ERROR_CODE_WRONG_ADDRESS;
        if (errorString.empty())
            errorString = std::string("WALLET_RPC_ERROR_CODE_WRONG_ADDRESS: ") + address;
        return false;
    }
    de.amount = amount;
    dsts.push_back(de);

    if (has_payment_id)
    {
        if (!payment_id.empty() || integrated_payment_id != cryptonote::null_hash8)
        {
//            er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
//            er.message = "A single payment id is allowed per transaction";
            return false;
        }
        integrated_payment_id = new_payment_id;
        cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, integrated_payment_id);

        /* Append Payment ID data into extra */
        if (!cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce))
        {
//            er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
//            er.message = "Something went wrong with integrated payment_id.";
            return false;
        }
    }

    if (!payment_id.empty())
    {
        /* Just to clarify */
        const std::string& payment_id_str = payment_id;

        crypto::hash long_payment_id;
        crypto::hash8 short_payment_id;

        /* Parse payment ID */
        if (tools::GraftWallet::parse_long_payment_id(payment_id_str, long_payment_id))
        {
            cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, long_payment_id);
        }
        /* or short payment ID */
        else if (tools::GraftWallet::parse_short_payment_id(payment_id_str, short_payment_id))
        {
            cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, short_payment_id);
        }
        else
        {
//            er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
//            er.message = "Payment id has invalid format: \"" + payment_id_str + "\", expected 16 or 64 character string";
            return false;
        }

        /* Append Payment ID data into extra */
        if (!cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce))
        {
//            er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
//            er.message = "Something went wrong with payment_id. Please check its format: \"" + payment_id_str + "\", expected 64-character string";
            return false;
        }
    }
    return true;
}

bool supernode::BaseClientProxy::add_extra_nonce_to_tx_extra(std::vector<uint8_t> &tx_extra,
                                                             const cryptonote::blobdata &extra_nonce)
{
    CHECK_AND_ASSERT_MES(extra_nonce.size() <= TX_EXTRA_NONCE_MAX_COUNT, false, "extra nonce could be 255 bytes max");
    size_t start_pos = tx_extra.size();
    tx_extra.resize(tx_extra.size() + 2 + extra_nonce.size());
    //write tag
    tx_extra[start_pos] = TX_EXTRA_NONCE;
    //write len
    ++start_pos;
    tx_extra[start_pos] = static_cast<uint8_t>(extra_nonce.size());
    //write data
    ++start_pos;
    memcpy(&tx_extra[start_pos], extra_nonce.data(), extra_nonce.size());
    return true;
}

std::unique_ptr<tools::GraftWallet> supernode::BaseClientProxy::initWallet(const string &account, const string &password, bool use_base64) const
{
    std::unique_ptr<tools::GraftWallet> wal;
    try
    {
        wal = tools::GraftWallet::createWallet(account, password, "",
                                               m_Servant->GetNodeIp(), m_Servant->GetNodePort(),
                                               m_Servant->GetNodeLogin(), m_Servant->IsTestnet(),
                                               use_base64);
        std::string lDataDir = tools::get_default_data_dir() + scWalletCachePath;
        if (!boost::filesystem::exists(lDataDir))
        {
            boost::filesystem::create_directories(lDataDir);
        }
        std::string lCacheFile = lDataDir +
                wal->get_account().get_public_address_str(wal->testnet());
        if (boost::filesystem::exists(lCacheFile))
        {
            wal->load_cache(lCacheFile);
        }
    }
    catch (const std::exception& e)
    {
        wal = nullptr;
    }
    return wal;
}

void supernode::BaseClientProxy::storeWalletState(tools::GraftWallet *wallet)
{
    if (wallet)
    {
        std::string lDataDir = tools::get_default_data_dir() + scWalletCachePath;
        if (!boost::filesystem::exists(lDataDir))
        {
            boost::filesystem::create_directories(lDataDir);
        }
        std::string lCacheFile = lDataDir +
                wallet->get_account().get_public_address_str(wallet->testnet());
        wallet->store_cache(lCacheFile);
    }
}

string supernode::BaseClientProxy::base64_decode(const string &encoded_data)
{
    return epee::string_encoding::base64_decode(encoded_data);
}

string supernode::BaseClientProxy::base64_encode(const string &data)
{
    return epee::string_encoding::base64_encode(data);
}
