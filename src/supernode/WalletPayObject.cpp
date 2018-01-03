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

#include "WalletPayObject.h"
#include "graft_defines.h"
#include "WalletProxy.h"

void supernode::WalletPayObject::Owner(WalletProxy* o) { m_Owner = o; }

bool supernode::WalletPayObject::OpenSenderWallet(const string &wallet, const string &walletPass)
{
    // m_Owner is WalletProxy here
    m_wallet = m_Owner->initWallet(wallet, walletPass);
    if (!m_wallet) {
        LOG_ERROR("Error initializing wallet");
        return false;
    }
    m_wallet->refresh();
    return true;
}

void supernode::WalletPayObject::BeforStart() {
	m_Status = NTransactionStatus::InProgress;
	ADD_RTA_OBJECT_HANDLER(GetPayStatus, rpc_command::WALLET_GET_TRANSACTION_STATUS, WalletPayObject);
}


bool supernode::WalletPayObject::Init(const rpc_command::WALLET_PAY::request& src) {
	bool ret = _Init(src);
    m_Status = ret ? NTransactionStatus::Success : NTransactionStatus::Fail;
	return ret;
}

bool supernode::WalletPayObject::_Init(const rpc_command::WALLET_PAY::request& src) {
	BaseRTAObject::Init(src);

    if( !OpenSenderWallet(m_Owner->base64_decode(src.Account), src.Password) ) { LOG_ERROR("!OpenSenderWallet"); return false; }

	// we allready have block num
	TransactionRecord.AuthNodes = m_Servant->GetAuthSample( TransactionRecord.BlockNum );
    if ( TransactionRecord.AuthNodes.empty() ) {
        LOG_ERROR("Failed to get auth sample");
        return false;
    }

	InitSubnet();

	vector<rpc_command::WALLET_PROXY_PAY::response> outv;
	rpc_command::WALLET_PROXY_PAY::request inbr;
	rpc_command::ConvertFromTR(inbr, TransactionRecord);
	string cwa = m_wallet->get_account().get_public_address_str(m_wallet->testnet());;
	string data = TransactionRecord.PaymentID + string(":") + cwa;
	inbr.CustomerWalletAddr = cwa;
	inbr.CustomerWalletSign = m_wallet->sign(data);

	//LOG_PRINT_L5("CWA: "<<data);

    if( !m_SubNetBroadcast.Send(dapi_call::WalletProxyPay, inbr, outv) || outv.empty() )  {
        LOG_ERROR("Failed to send WalletProxyPay broadcast");
        return false;
    }

    LOG_PRINT_L2("obtained " << outv.size() << " WALLET_PROXY_PAY responses");
    if (outv.size() == 0) {
        LOG_ERROR("NO WALLET_PROXY_PAY responses obtained");
        return false;
    }

    if (outv.size() != m_Servant->AuthSampleSize()) {
        LOG_ERROR("outv.size != AuthSampleSize");
        return false;// not all signs gotted
    }

    for (auto& a : outv) {

		if( !CheckSign(a.FSN_StakeWalletAddr, a.Sign) ) return false;
		m_Signs.push_back(a.Sign);
        LOG_PRINT_L0("pushing sign " << a.Sign << " to tx,  checked with address: " << a.FSN_StakeWalletAddr);

	}



	if( !PutTXToPool() ) return false;

	rpc_command::WALLET_PUT_TX_IN_POOL::request req;
	req.PaymentID = TransactionRecord.PaymentID;
	req.TransactionPoolID = m_TransactionPoolID;
	//LOG_PRINT_L5("PaymentID: "<<TransactionRecord.PaymentID);

	vector<rpc_command::WALLET_PUT_TX_IN_POOL::response> vv_out;

	if( !m_SubNetBroadcast.Send( dapi_call::WalletPutTxInPool, req, vv_out) ) return false;


	return true;
}

bool supernode::WalletPayObject::GetPayStatus(const rpc_command::WALLET_GET_TRANSACTION_STATUS::request& in, rpc_command::WALLET_GET_TRANSACTION_STATUS::response& out) {
	out.Status = int(m_Status);
	//TimeMark -= boost::posix_time::hours(3);
    out.Result = STATUS_OK;
    return true;
}




bool supernode::WalletPayObject::PutTXToPool() {
	// TODO: IMPL. all needed data we have in TransactionRecord + m_Signs.
	// TODO: Result, monero_tranaction_id must be putted to m_TransactionPoolID

    // TODO: send tx to blockchain
    // Things we need here here

    // 1. destination address:
    // TransactionRecord.POSAddress;
    // 2. amount
    // TransactionRecord.Amount;
    // 3. wallet -> we opened it previously with OpenSenderWallet
    if (!m_wallet) {
        LOG_ERROR("Wallet needs to be opened with OpenSenderWallet before this call");
        return false;
    }

    GraftTxExtra tx_extra;
    tx_extra.BlockNum = 123;
    tx_extra.PaymentID = TransactionRecord.PaymentID;
    tx_extra.Signs = m_Signs;

//    for (auto &sign : tx_extra.Signs) {
//        LOG_PRINT_L0("pushing sign to tx extra: " << sign);
//    }

    std::unique_ptr<PendingTransaction> ptx {
            m_wallet->createTransaction(TransactionRecord.POSAddress,
                                      "",
                                      TransactionRecord.Amount,
                                      0,
                                      tx_extra,
                                      Monero::PendingTransaction::Priority_Medium
                                      )};

    if (ptx->status() != PendingTransaction::Status_Ok) {
        LOG_ERROR("Failed to create tx: " << ptx->errorString());
        return false;
    }

    if (!ptx->commit()) {
        LOG_ERROR("Failed to send tx: " << ptx->errorString());
        return false;
    }

    if (ptx->txCount() == 0) {
        LOG_ERROR("Interlal error: txCount == 0");
        return false;
    }
    if (ptx->txCount() > 1) {
        LOG_ERROR("TODO: we should handle this somehow");
        throw std::runtime_error(std::string("tx was splitted by ") + std::to_string(ptx->txCount()) + " transactions, we dont hadle it now");
    }

    m_TransactionPoolID = ptx->txid()[0];
    return true;
}


