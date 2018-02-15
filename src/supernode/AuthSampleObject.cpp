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

#include "AuthSampleObject.h"
#include "graft_wallet.h"

void supernode::AuthSampleObject::Owner(AuthSample* o) { m_Owner = o; }

bool supernode::AuthSampleObject::Init(const RTA_TransactionRecord& src) {
	TransactionRecord = src;

	ADD_RTA_OBJECT_HANDLER(WalletProxyGetPosData, rpc_command::WALLET_GET_POS_DATA, AuthSampleObject);
	ADD_RTA_OBJECT_HANDLER(WalletProxyRejectPay, rpc_command::WALLET_REJECT_PAY, AuthSampleObject);

	return true;
}


bool supernode::AuthSampleObject::WalletProxyPay(const rpc_command::WALLET_PROXY_PAY::request& inp, rpc_command::WALLET_PROXY_PAY::response& out, epee::json_rpc::error &er) {
	RTA_TransactionRecord src;
	rpc_command::ConvertToTR(src, inp, m_Servant);
	if(src!=TransactionRecord) { LOG_PRINT_L5("not eq records"); return false; }

	string data = TransactionRecord.PaymentID + string(":") + inp.CustomerWalletAddr;
	bool signok = tools::GraftWallet::verifySignedMessage(data, inp.CustomerWalletAddr, inp.CustomerWalletSign, m_Servant->IsTestnet());
	//LOG_PRINT_L5("Check sign: "<<signok<<"  data: "<<data<<"  sign: "<<inp.CustomerWalletSign);


	//LOG_PRINT_L5("PaymentID: "<<TransactionRecord.PaymentID<<"  m_ReadyForDelete: "<<m_ReadyForDelete);

	// TODO: send LOCK. WTF?? all our nodes got this packet by sub-net broadcast. so only top node must send broad cast

	ADD_RTA_OBJECT_HANDLER(WalletPutTxInPool, rpc_command::WALLET_PUT_TX_IN_POOL, AuthSampleObject);

	out.Sign = GenerateSignForTransaction();
	out.FSN_StakeWalletAddr = m_Servant->GetMyStakeWallet().Addr;

	return true;
}

bool supernode::AuthSampleObject::WalletPutTxInPool(const rpc_command::WALLET_PUT_TX_IN_POOL::request& in, rpc_command::WALLET_PUT_TX_IN_POOL::response& out, epee::json_rpc::error &er) {
	// all ok, notify PoS about this
	rpc_command::POS_TR_SIGNED::request req;
	rpc_command::POS_TR_SIGNED::response resp;
	req.TransactionPoolID = in.TransactionPoolID;
	if( !SendDAPICall(PosIP, PosPort, dapi_call::PoSTRSigned, req, resp) ) return false;


	return true;
}

bool supernode::AuthSampleObject::WalletProxyGetPosData(const rpc_command::WALLET_GET_POS_DATA::request& in, rpc_command::WALLET_GET_POS_DATA::response& out, epee::json_rpc::error &er) {
    out.POSSaleDetails = TransactionRecord.POSSaleDetails;
	return true;
}

bool supernode::AuthSampleObject::WalletProxyRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out, epee::json_rpc::error &er) {
	rpc_command::WALLET_REJECT_PAY::request in2 = in;
	bool ret = SendDAPICall(PosIP, PosPort, dapi_call::AuthWalletRejectPay, in2, out);
	return ret;
}

string supernode::AuthSampleObject::GenerateSignForTransaction() {
	string sign = m_Servant->SignByWalletPrivateKey( TransactionRecord.MessageForSign(), m_Servant->GetMyStakeWallet().Addr );
	//LOG_PRINT_L5("GenerateSignForTransaction: mes: "<<TransactionRecord.MessageForSign()<<"  addr: "<<m_Servant->GetMyStakeWallet().Addr<<"  sign: "<<sign)
	return sign;
}


bool supernode::AuthSampleObject::Init(const RTA_TransactionRecordBase& src) { return false; }




