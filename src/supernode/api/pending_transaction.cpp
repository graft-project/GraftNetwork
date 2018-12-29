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

#include "pending_transaction.h"
#include "supernode/wallet_errors.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"

#include <memory>
#include <vector>
#include <sstream>
#include <boost/format.hpp>

using namespace std;

namespace Monero {


GraftPendingTransactionImpl::GraftPendingTransactionImpl(tools::GraftWallet *graft_wallet)
    : mWallet(graft_wallet)
{
  m_status = Status_Ok;
}

GraftPendingTransactionImpl::~GraftPendingTransactionImpl()
{

}

int GraftPendingTransactionImpl::status() const
{
    return m_status;
}

string GraftPendingTransactionImpl::errorString() const
{
    return m_errorString;
}

std::vector<std::string> GraftPendingTransactionImpl::txid() const
{
    std::vector<std::string> txid;
    for (const auto &pt: m_pending_tx)
        txid.push_back(epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(pt.tx)));
    return txid;
}

bool GraftPendingTransactionImpl::commit(const std::string &filename, bool overwrite)
{

    LOG_PRINT_L3("m_pending_tx size: " << m_pending_tx.size());

    try {
      // Save tx to file
      if (!filename.empty()) {
        boost::system::error_code ignore;
        bool tx_file_exists = boost::filesystem::exists(filename, ignore);
        if(tx_file_exists && !overwrite){
          m_errorString = string("Attempting to save transaction to file, but specified file(s) exist. Exiting to not risk overwriting. File:") + filename;
          m_status = Status_Error;
          LOG_ERROR(m_errorString);
          return false;
        }
        bool r = mWallet->save_tx(m_pending_tx, filename);
        if (!r) {
          m_errorString = "Failed to write transaction(s) to file";
          m_status = Status_Error;
        } else {
          m_status = Status_Ok;
        }
      }
      // Commit tx
      else {
        auto reverse_it = m_pending_tx.rbegin();
        for (; reverse_it != m_pending_tx.rend(); ++reverse_it) {
              mWallet->commit_tx(*reverse_it);
        }
      }
    } catch (const tools::error::daemon_busy&) {
        // TODO: make it translatable with "tr"?
        m_errorString = "daemon is busy. Please try again later.";
        m_status = Status_Error;
    } catch (const tools::error::no_connection_to_daemon&) {
        m_errorString = "no connection to daemon. Please make sure daemon is running.";
        m_status = Status_Error;
    } catch (const tools::error::tx_rejected& e) {
        std::ostringstream writer(m_errorString);
        writer << (boost::format("transaction %s was rejected by daemon with status: ") % get_transaction_hash(e.tx())) <<  e.status();
        std::string reason = e.reason();
        m_status = Status_Error;
        m_errorString = writer.str();
        if (!reason.empty())
          m_errorString  += string(". Reason: ") + reason;
    } catch (const std::exception &e) {
        m_errorString = string("Unknown exception: ") + e.what();
        m_status = Status_Error;
    } catch (...) {
        m_errorString = "Unhandled exception";
        LOG_ERROR(m_errorString);
        m_status = Status_Error;
    }

    return m_status == Status_Ok;
}

uint64_t GraftPendingTransactionImpl::amount() const
{
    uint64_t result = 0;
    for (const auto &ptx : m_pending_tx)   {
        for (const auto &dest : ptx.dests) {
            result += dest.amount;
        }
    }
    return result;
}

uint64_t GraftPendingTransactionImpl::dust() const
{
    uint64_t result = 0;
    for (const auto & ptx : m_pending_tx) {
        result += ptx.dust;
    }
    return result;
}

uint64_t GraftPendingTransactionImpl::fee() const
{
    uint64_t result = 0;
    for (const auto &ptx : m_pending_tx) {
        result += ptx.fee;
    }
    return result;
}

uint64_t GraftPendingTransactionImpl::txCount() const
{
    return m_pending_tx.size();
}

std::vector<uint32_t> GraftPendingTransactionImpl::subaddrAccount() const
{
    std::vector<uint32_t> result;
    for (const auto& ptx : m_pending_tx)
        result.push_back(ptx.construction_data.subaddr_account);
    return result;
}

std::vector<std::set<uint32_t>> GraftPendingTransactionImpl::subaddrIndices() const
{
    std::vector<std::set<uint32_t>> result;
    for (const auto& ptx : m_pending_tx)
        result.push_back(ptx.construction_data.subaddr_indices);
    return result;
}


void GraftPendingTransactionImpl::setPendingTx(std::vector<tools::GraftWallet::pending_tx> pending_tx)
{
    m_pending_tx = pending_tx;
}

void GraftPendingTransactionImpl::setStatus(int status)
{
    m_status = status;
}

void GraftPendingTransactionImpl::setErrorString(const string &message)
{
    m_errorString = message;
}

std::string GraftPendingTransactionImpl::multisigSignData()
{
  /*
    try
    {
        if(!m_wallet.multisig().isMultisig)
        {
            throw std::runtime_error("wallet is not multisig");
        }

        tools::wallet2::multisig_tx_set txSet;
        txSet.m_ptx = m_pending_tx;
        txSet.m_signers = m_signers;
        auto cipher = m_wallet.m_wallet->save_multisig_tx(txSet);

        return epee::string_tools::buff_to_hex_nodelimer(cipher);
    }
    catch (const std::exception& e)
    {
        m_status = Status_Error;
        m_errorString = std::string(tr("Couldn't multisig sign data: ")) + e.what();
    }

    */
    return std::string();
}

void GraftPendingTransactionImpl::signMultisigTx()
{
  /*
    try
    {
        std::vector<crypto::hash> ignore;

        tools::wallet2::multisig_tx_set txSet;
        txSet.m_ptx = m_pending_tx;
        txSet.m_signers = m_signers;

        if (!m_wallet.m_wallet->sign_multisig_tx(txSet, ignore)) {
            throw std::runtime_error("couldn't sign multisig transaction");
        }

        std::swap(m_pending_tx, txSet.m_ptx);
        std::swap(m_signers, txSet.m_signers);
    }
    catch(const std::exception& e)
    {
        m_status = Status_Error;
        m_errorString = std::string(tr("Couldn't sign multisig transaction: ")) + e.what();
    }
    */
}

std::vector<std::string> GraftPendingTransactionImpl::signersKeys() const
{
    std::vector<std::string> keys;
    /*
    keys.reserve(m_signers.size());

    for(const auto& signer: m_signers)
    {
        keys.emplace_back(tools::base58::encode(cryptonote::t_serializable_object_to_blob(signer)));
    }
    */
    return keys;
}

}

namespace Bitmonero = Monero;

