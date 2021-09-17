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

#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "mnemonics/electrum-words.h"
#include "common/command_line.h"
#include "wallet/wallet_errors.h"
#include "baseclientproxy.h"
#include "graft_defines.h"
#include "string_coding.h"
#include "common/util.h"

static const std::string scWalletCachePath("/cache/");

namespace  {
void set_confirmations(tools::wallet_rpc::transfer_entry &entry, uint64_t blockchain_height, uint64_t block_reward)
{
  if (entry.height >= blockchain_height)
  {
    entry.confirmations = 0;
    entry.suggested_confirmations_threshold = 0;
    return;
  }
  entry.confirmations = blockchain_height - entry.height;
  if (block_reward == 0)
    entry.suggested_confirmations_threshold = 0;
  else
    entry.suggested_confirmations_threshold = (entry.amount + block_reward - 1) / block_reward;
}

void fill_transfer_entry(tools::GraftWallet * wallet, tools::wallet_rpc::transfer_entry &entry, const crypto::hash &txid, const crypto::hash &payment_id, const tools::wallet2::payment_details &pd)
{
  entry.txid = epee::string_tools::pod_to_hex(pd.m_tx_hash);
  entry.payment_id = epee::string_tools::pod_to_hex(payment_id);
  if (entry.payment_id.substr(16).find_first_not_of('0') == std::string::npos)
    entry.payment_id = entry.payment_id.substr(0,16);
  entry.height = pd.m_block_height;
  entry.timestamp = pd.m_timestamp;
  entry.amount = pd.m_amount;
  entry.unlock_time = pd.m_unlock_time;
  entry.fee = pd.m_fee;
  entry.note = wallet->get_tx_note(pd.m_tx_hash);
  entry.type = pay_type_string(pd.m_type);
  entry.subaddr_index = pd.m_subaddr_index;
  entry.address = wallet->get_subaddress_as_str(pd.m_subaddr_index);
  set_confirmations(entry, wallet->get_blockchain_current_height(), wallet->get_last_block_reward());
}
//------------------------------------------------------------------------------------------------------------------------------
void fill_transfer_entry(tools::GraftWallet * wallet, tools::wallet_rpc::transfer_entry &entry, const crypto::hash &txid, const tools::wallet2::confirmed_transfer_details &pd)
{
  entry.txid = epee::string_tools::pod_to_hex(txid);
  entry.payment_id = epee::string_tools::pod_to_hex(pd.m_payment_id);
  if (entry.payment_id.substr(16).find_first_not_of('0') == std::string::npos)
    entry.payment_id = entry.payment_id.substr(0,16);
  entry.height = pd.m_block_height;
  entry.timestamp = pd.m_timestamp;
  entry.unlock_time = pd.m_unlock_time;
  entry.fee = pd.m_amount_in - pd.m_amount_out;
  uint64_t change = pd.m_change == (uint64_t)-1 ? 0 : pd.m_change; // change may not be known
  entry.amount = pd.m_amount_in - change - entry.fee;
  entry.note = wallet->get_tx_note(txid);

  for (const auto &d: pd.m_dests) {
    entry.destinations.push_back(tools::transfer_destination());
    tools::transfer_destination &td = entry.destinations.back();
    td.amount = d.amount;
    td.address = get_account_address_as_str(wallet->nettype(), d.is_subaddress, d.addr);
  }

  entry.type = "out";
  entry.subaddr_index = { pd.m_subaddr_account, 0 };
  entry.address = wallet->get_subaddress_as_str({pd.m_subaddr_account, 0});
  set_confirmations(entry, wallet->get_blockchain_current_height(), wallet->get_last_block_reward());
}
//------------------------------------------------------------------------------------------------------------------------------
void fill_transfer_entry(tools::GraftWallet * wallet, tools::wallet_rpc::transfer_entry &entry, const crypto::hash &txid, const tools::wallet2::unconfirmed_transfer_details &pd)
{
  bool is_failed = pd.m_state == tools::wallet2::unconfirmed_transfer_details::failed;
  entry.txid = epee::string_tools::pod_to_hex(txid);
  entry.payment_id = epee::string_tools::pod_to_hex(pd.m_payment_id);
  entry.payment_id = epee::string_tools::pod_to_hex(pd.m_payment_id);
  if (entry.payment_id.substr(16).find_first_not_of('0') == std::string::npos)
    entry.payment_id = entry.payment_id.substr(0,16);
  entry.height = 0;
  entry.timestamp = pd.m_timestamp;
  entry.fee = pd.m_amount_in - pd.m_amount_out;
  entry.amount = pd.m_amount_in - pd.m_change - entry.fee;
  entry.unlock_time = pd.m_tx.unlock_time;
  entry.note = wallet->get_tx_note(txid);
  entry.type = is_failed ? "failed" : "pending";
  entry.subaddr_index = { pd.m_subaddr_account, 0 };
  entry.address = wallet->get_subaddress_as_str({pd.m_subaddr_account, 0});
  set_confirmations(entry, wallet->get_blockchain_current_height(), wallet->get_last_block_reward());
}
//------------------------------------------------------------------------------------------------------------------------------
void fill_transfer_entry(tools::GraftWallet * wallet, tools::wallet_rpc::transfer_entry &entry, const crypto::hash &payment_id, const tools::wallet2::pool_payment_details &ppd)
{
  const tools::wallet2::payment_details &pd = ppd.m_pd;
  entry.txid = epee::string_tools::pod_to_hex(pd.m_tx_hash);
  entry.payment_id = epee::string_tools::pod_to_hex(payment_id);
  if (entry.payment_id.substr(16).find_first_not_of('0') == std::string::npos)
    entry.payment_id = entry.payment_id.substr(0,16);
  entry.height = 0;
  entry.timestamp = pd.m_timestamp;
  entry.amount = pd.m_amount;
  entry.unlock_time = pd.m_unlock_time;
  entry.fee = pd.m_fee;
  entry.note = wallet->get_tx_note(pd.m_tx_hash);
  entry.double_spend_seen = ppd.m_double_spend_seen;
  entry.type = "pool";
  entry.subaddr_index = pd.m_subaddr_index;
  entry.address = wallet->get_subaddress_as_str(pd.m_subaddr_index);
  set_confirmations(entry, wallet->get_blockchain_current_height(), wallet->get_last_block_reward());
}
}

template<class Src, class Dst>
void copy_seed(const Src& src, Dst& dst)
{
    epee::wipeable_string seed;
    src.get_seed(seed);
    dst.Seed = std::string(seed.data(), seed.size());
}

supernode::BaseClientProxy::BaseClientProxy()
{
}

void supernode::BaseClientProxy::Init()
{
    m_DAPIServer->ADD_DAPI_HANDLER(GetWalletBalance, rpc_command::GET_WALLET_BALANCE, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(GetWalletTransactions, rpc_command::GET_WALLET_TRANSACTIONS, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(CreateAccount, rpc_command::CREATE_ACCOUNT, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(GetSeed, rpc_command::GET_SEED, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(RestoreAccount, rpc_command::RESTORE_ACCOUNT, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(Transfer, rpc_command::TRANSFER, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(GetTransferFee, rpc_command::GET_TRANSFER_FEE, BaseClientProxy);
}

bool supernode::BaseClientProxy::GetWalletBalance(const supernode::rpc_command::GET_WALLET_BALANCE::request &in, supernode::rpc_command::GET_WALLET_BALANCE::response &out)
{
	LOG_PRINT_L0("BaseClientProxy::GetWalletBalance" << in.Account);
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(in.Account), in.Password, false);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    try
    {
        wal->refresh(wal->is_trusted_daemon());
        out.Balance = wal->balance_all();
        out.UnlockedBalance = wal->unlocked_balance_all();
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




bool supernode::BaseClientProxy::GetWalletTransactions(const supernode::rpc_command::GET_WALLET_TRANSACTIONS::request &in, supernode::rpc_command::GET_WALLET_TRANSACTIONS::response &out)
{
    MINFO("BaseClientProxy::GetWalletTransactions: " << in.Account);
    std::unique_ptr<tools::GraftWallet> wallet = initWallet(base64_decode(in.Account), in.Password, false);
    MINFO("BaseClientProxy::GetWalletTransactions: initWallet done");
    if (!wallet)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    try
    {
        // copy-pasted from wallet_rpc_server.cpp
        // TODO: refactor to avoid code duplication or use wallet2_api.h interfaces
        MINFO("BaseClientProxy::GetWalletTransactions: about to call 'refresh()'");
        wallet->refresh(wallet->is_trusted_daemon());
        MINFO("BaseClientProxy::GetWalletTransactions: 'refresh()' done");
        // incoming
        
        {
          std::list<std::pair<crypto::hash, tools::wallet2::payment_details>> payments;
          wallet->get_payments(payments, in.MinHeight, in.MaxHeight, in.AccountIndex, in.SubaddrIndices);
          for (std::list<std::pair<crypto::hash, tools::wallet2::payment_details>>::const_iterator i = payments.begin(); i != payments.end(); ++i) {
            out.TransfersIn.push_back(tools::wallet_rpc::transfer_entry());
            fill_transfer_entry(wallet.get(), out.TransfersIn.back(), i->second.m_tx_hash, i->first, i->second);
          }
        }
        MINFO("BaseClientProxy::GetWalletTransactions: 'incoming payments' done");
    
        // outgoing
        {
          std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>> payments;
          wallet->get_payments_out(payments, in.MinHeight, in.MaxHeight, in.AccountIndex, in.SubaddrIndices);
          for (std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>>::const_iterator i = payments.begin(); i != payments.end(); ++i) {
            out.TransfersOut.push_back(tools::wallet_rpc::transfer_entry());
            fill_transfer_entry(wallet.get(), out.TransfersOut.back(), i->first, i->second);
          }
        }
        MINFO("BaseClientProxy::GetWalletTransactions: 'outgoing payments' done");
        // pending or failed
        {
          std::list<std::pair<crypto::hash, tools::wallet2::unconfirmed_transfer_details>> upayments;
          wallet->get_unconfirmed_payments_out(upayments, in.AccountIndex, in.SubaddrIndices);
          for (std::list<std::pair<crypto::hash, tools::wallet2::unconfirmed_transfer_details>>::const_iterator i = upayments.begin(); i != upayments.end(); ++i) {
            const tools::wallet2::unconfirmed_transfer_details &pd = i->second;
            bool is_failed = pd.m_state == tools::wallet2::unconfirmed_transfer_details::failed;
            std::list<tools::wallet_rpc::transfer_entry> &entries = is_failed ? out.TransfersFailed : out.TransfersPending;
            entries.push_back(tools::wallet_rpc::transfer_entry());
            fill_transfer_entry(wallet.get(), entries.back(), i->first, i->second);
          }
        }
        MINFO("BaseClientProxy::GetWalletTransactions: 'unconfirmed payments' done");
        // pool
        {
          wallet->update_pool_state();
    
          std::list<std::pair<crypto::hash, tools::wallet2::pool_payment_details>> payments;
          wallet->get_unconfirmed_payments(payments, in.AccountIndex, in.SubaddrIndices);
          for (std::list<std::pair<crypto::hash, tools::wallet2::pool_payment_details>>::const_iterator i = payments.begin(); i != payments.end(); ++i) {
            out.TransfersPool.push_back(tools::wallet_rpc::transfer_entry());
            fill_transfer_entry(wallet.get(), out.TransfersPool.back(), i->first, i->second);
          }
        }
        MINFO("BaseClientProxy::GetWalletTransactions: 'pool payments' done");
        
        storeWalletState(wallet.get());
    }
    catch (const std::exception& e)
    {
        MERROR("Exception while retrieving transaction history: " << e.what());
        out.Result = ERROR_TX_HISTORY_NOT_AVAILABLE;
        return false;
    }
    MINFO("Returning transactions: " << out.TransfersIn.size() + out.TransfersOut.size() + out.TransfersFailed.size()
          + out.TransfersPending.size());
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
                                             m_Servant->GetNodeLogin(), m_Servant->nettype());
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
    out.Address = wal->get_account().get_public_address_str(wal->nettype());
    out.ViewKey = epee::string_tools::pod_to_hex(wal->get_account().get_keys().m_view_secret_key);
    copy_seed(*wal, out);
    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::GetSeed(const supernode::rpc_command::GET_SEED::request &in, supernode::rpc_command::GET_SEED::response &out)
{
	LOG_PRINT_L0("BaseClientProxy::GetSeed" << in.Account);
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(in.Account), in.Password, false);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    wal->set_seed_language(in.Language);
    copy_seed(*wal, out);
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
                                         m_Servant->GetNodeLogin(), m_Servant->nettype());
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
        out.Address = wal->get_account().get_public_address_str(wal->nettype());
        out.ViewKey = epee::string_tools::pod_to_hex(
                    wal->get_account().get_keys().m_view_secret_key);
        copy_seed(*wal, out);
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
    std::unique_ptr<tools::GraftWallet> wallet = initWallet(base64_decode(in.Account), in.Password, false);
    if (!wallet)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    std::vector<cryptonote::tx_destination_entry> dsts;
    std::vector<uint8_t> extra;

    std::string payment_id = in.PaymentID;

    uint64_t amount;
    std::istringstream iss(in.Amount);
    iss >> amount;

    // validate the transfer requested and populate dsts & extra
    if (!validate_transfer(wallet.get(), in.Address, amount, payment_id, dsts, extra))
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    try
    {
        uint64_t mixin = CRYPTONOTE_DEFAULT_TX_MIXIN;
        uint64_t unlock_time = 0;
        uint64_t priority = 0;
        uint32_t subaddr_count = 0;
        std::set<uint32_t> subaddr_indices;
        boost::optional<uint8_t> hf_version = wallet->get_hard_fork_version();
        cryptonote::loki_construct_tx_params tx_params = tools::wallet2::construct_params(*hf_version, cryptonote::txtype::standard, priority);
        std::vector<tools::GraftWallet::pending_tx> ptx_vector =
                wallet->create_transactions_2(dsts, mixin, unlock_time, priority, extra, subaddr_count, subaddr_indices, tx_params);

        uint64_t fees = 0;
        for (const auto ptx : ptx_vector)
        {
            fees += ptx.fee;
        }
        if (fees == 0)
        {
            out.Result = ERROR_OPEN_WALLET_FAILED;
            return false;
        }
        out.Fee = fees;
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
    std::unique_ptr<tools::GraftWallet> wal = initWallet(base64_decode(in.Account), in.Password, false);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }

    uint64_t amount;
    std::istringstream iss(in.Amount);
    iss >> amount;

    out.Result = create_transfer(wal.get(), in.Address, amount, in.PaymentID);
    if (out.Result == STATUS_OK)
    {
        storeWalletState(wal.get());
        return true;
    }
    return false;
}

bool supernode::BaseClientProxy::validate_transfer(tools::GraftWallet *wallet,
                                                   const string &address, uint64_t amount,
                                                   const string payment_id,
                                                   std::vector<cryptonote::tx_destination_entry> &dsts,
                                                   std::vector<uint8_t> &extra)
{
    if (!wallet)
    {
        return false;
    }
    cryptonote::tx_destination_entry de;
    //Based on PendingTransaction *WalletImpl::createTransaction()
    cryptonote::address_parse_info addr;
    if(!cryptonote::get_account_address_from_str(addr, wallet->nettype(), address))
    {
        // TODO: copy-paste 'if treating as an address fails, try as url' from simplewallet.cpp:1982
//        m_status = Status_Error;
//        m_errorString = "Invalid destination address";
        return false;
    }
    de.addr = addr.address;
    de.amount = amount;
    dsts.push_back(de);

    // if dst_addr is not an integrated address, parse payment_id
    if (!addr.has_payment_id && !payment_id.empty())
    {
        // copy-pasted from simplewallet.cpp:2212
        crypto::hash payment_id_long;
        bool r = tools::GraftWallet::parse_long_payment_id(payment_id, payment_id_long);
        if (r)
        {
            std::string extra_nonce;
            cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_long);
            r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
        }
        else
        {
            r = tools::GraftWallet::parse_short_payment_id(payment_id, addr.payment_id);
            if (r)
            {
                std::string extra_nonce;
                cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, addr.payment_id);
                r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
            }
        }
        if (!r)
        {
//            m_status = Status_Error;
//            m_errorString = tr("payment id has invalid format, expected 16 or 64 character hex string: ") + payment_id;
            return false;
        }
    }
    else if (addr.has_payment_id)
    {
        std::string extra_nonce;
        cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, addr.payment_id);
        bool r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
        if (!r)
        {
//            m_status = Status_Error;
//            m_errorString = tr("Failed to add short payment id: ") + epee::string_tools::pod_to_hex(payment_id_short);
            return false;
        }
    }
    return true;
}

std::unique_ptr<tools::GraftWallet> supernode::BaseClientProxy::initWallet(const string &account, const string &password, bool use_base64) const
{
    std::unique_ptr<tools::GraftWallet> wal;
    try
    {
        wal = tools::GraftWallet::createWallet(account, password, "", m_Servant->GetNodeIp(),
                                               m_Servant->GetNodePort(), m_Servant->GetNodeLogin(),
                                               m_Servant->nettype(), use_base64);
        std::string lDataDir = tools::get_default_data_dir() + scWalletCachePath;
        if (!boost::filesystem::exists(lDataDir))
        {
            boost::filesystem::create_directories(lDataDir);
        }
        std::string lCacheFile = lDataDir +
                wal->get_account().get_public_address_str(wal->nettype());
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
                wallet->get_account().get_public_address_str(wallet->nettype());
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

int supernode::BaseClientProxy::create_transfer(tools::GraftWallet *wallet, const std::string &address,
                                                const uint64_t &amount, const std::string &payment_id)
{
    std::vector<cryptonote::tx_destination_entry> dsts;
    std::vector<uint8_t> extra;

    std::string paymentID = payment_id;

    // validate the transfer requested and populate dsts & extra
    if (!validate_transfer(wallet, address, amount, paymentID, dsts, extra))
    {
        return ERROR_OPEN_WALLET_FAILED;
    }

    try
    {
        uint64_t mixin = CRYPTONOTE_DEFAULT_TX_MIXIN;
        uint64_t unlock_time = 0;
        uint64_t priority = 0;
        bool do_not_relay = false;
        uint32_t subaddr_count = 0;
        std::set<uint32_t> subaddr_indices;
        boost::optional<uint8_t> hf_version = wallet->get_hard_fork_version();
        cryptonote::loki_construct_tx_params tx_params = tools::wallet2::construct_params(*hf_version, cryptonote::txtype::standard, priority);
        std::vector<tools::GraftWallet::pending_tx> ptx_vector =
                wallet->create_transactions_2(dsts, mixin, unlock_time, priority, extra,
                                              subaddr_count, subaddr_indices, tx_params);

        if (!do_not_relay && ptx_vector.size() > 0)
        {
            wallet->commit_tx(ptx_vector);
        }
    }
    catch (const tools::error::daemon_busy& e)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY;
        //      er.message = e.what();
        return ERROR_OPEN_WALLET_FAILED;
    }
    catch (const std::exception& e)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
        //      er.message = e.what();
        return ERROR_OPEN_WALLET_FAILED;
    }
    catch (...)
    {
        //      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
        //      er.message = "WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR";
        return ERROR_OPEN_WALLET_FAILED;
    }
    return STATUS_OK;
}
