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

#include "AuthSample.h"



void supernode::AuthSample::Init()  {
	m_DAPIServer->ADD_DAPI_HANDLER(PosProxySale, rpc_command::POS_PROXY_SALE, AuthSample);
	m_DAPIServer->ADD_DAPI_HANDLER(WalletProxyPay, rpc_command::WALLET_PROXY_PAY, AuthSample);
}


supernode::DAPICallResult supernode::AuthSample::PosProxySale(const rpc_command::POS_PROXY_SALE::request& in, rpc_command::POS_PROXY_SALE::response& out) {
	RTA_TransactionRecord tr;
	rpc_command::ConvertToTR(tr, in, m_Servant);

	if( !Check(tr) ) return "Check incoming transaction record fail";

	boost::shared_ptr<AuthSampleObject> data = boost::shared_ptr<AuthSampleObject>( new AuthSampleObject() );
	data->Owner(this);
	Setup(data);
	DAPICallResult ret = data->Init(tr);
	if( ret!="" ) return string("Can't init AuthSampleObject: ")+ret;

	data->PosIP = in.SenderIP;
	data->PosPort = in.SenderPort;

	Add(data);

	//LOG_PRINT_L5("ADD: "<<in.PaymentID<<"  in: "<<m_DAPIServer->Port());

	return "";
}

supernode::DAPICallResult supernode::AuthSample::WalletProxyPay(const rpc_command::WALLET_PROXY_PAY::request& in, rpc_command::WALLET_PROXY_PAY::response& out) {
	boost::shared_ptr<BaseRTAObject> ff = ObjectByPayment(in.PaymentID);
	boost::shared_ptr<AuthSampleObject> data = boost::dynamic_pointer_cast<AuthSampleObject>(ff);
	if(!data) { return string("not found object: ") + boost::lexical_cast<string>(in.PaymentID) + string("  in: ") + boost::lexical_cast<string>(m_DAPIServer->Port()); }

	DAPICallResult ret = data->WalletProxyPay(in, out);
	if( ret!="" ) { Remove(data); return ret; }

	return "";
}


bool supernode::AuthSample::Check(RTA_TransactionRecord& tr) {
	// TODO: IMPL check sample selection if ok for givved block number
	return true;
}













