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

#include "graft_defines.h"
#include "WalletProxy.h"

void supernode::WalletProxy::Init() {
    BaseClientProxy::Init();
    m_Work.Workers(10);
    m_DAPIServer->ADD_DAPI_HANDLER(Pay, rpc_command::WALLET_PAY, WalletProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(WalletGetPosData, rpc_command::WALLET_GET_POS_DATA, WalletProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(WalletRejectPay, rpc_command::WALLET_REJECT_PAY, WalletProxy);
}

bool supernode::WalletProxy::WalletRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out, epee::json_rpc::error &er) {
	// TODO: if have PayID, don't call

	SubNetBroadcast sub;
	sub.Set( m_DAPIServer, in.PaymentID, m_Servant->GetAuthSample(in.BlockNum) );
	vector<rpc_command::WALLET_REJECT_PAY::response> vout;
	bool ret = sub.Send( dapi_call::WalletProxyRejectPay, in, vout );
    out.Result = ret ? STATUS_OK : ERROR_CANNOT_REJECT_PAY;
	return ret;
}


bool supernode::WalletProxy::Pay(const rpc_command::WALLET_PAY::request& in, rpc_command::WALLET_PAY::response& out, epee::json_rpc::error &er) {
	boost::shared_ptr<WalletPayObject> data = boost::shared_ptr<WalletPayObject>( new WalletPayObject() );
	data->Owner(this);
	Setup(data);
	data->BeforStart();
	Add(data);

	m_Work.Service.post( [data, in](){
	    if (!data->Init(in)) {
	        LOG_ERROR("Failed to init WalletPayObject");
	        return;
	    }
	} );

    return true;
}

bool supernode::WalletProxy::WalletGetPosData(const rpc_command::WALLET_GET_POS_DATA::request& in, rpc_command::WALLET_GET_POS_DATA::response& out, epee::json_rpc::error &er) {
	// we allready have block num
	vector< boost::shared_ptr<FSN_Data> > vv = m_Servant->GetAuthSample( in.BlockNum );
	if( vv.size()!=m_Servant->AuthSampleSize() ) return false;

	boost::shared_ptr<FSN_Data> data = *vv.begin();

	DAPI_RPC_Client call;
	call.Set(data->IP, data->Port);
	return call.Invoke(dapi_call::WalletProxyGetPosData, in, out);
}
