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

void supernode::AuthSample::Init()
{
    m_DAPIServer->ADD_DAPI_HANDLER(PosProxySale, rpc_command::POS_PROXY_SALE, AuthSample);
    m_DAPIServer->ADD_DAPI_HANDLER(WalletProxyPay, rpc_command::WALLET_PROXY_PAY, AuthSample);
}


bool supernode::AuthSample::PosProxySale(const rpc_command::POS_PROXY_SALE::request& in, rpc_command::POS_PROXY_SALE::response& out, epee::json_rpc::error &er)
{
    RTA_TransactionRecord tr;
    rpc_command::ConvertToTR(tr, in, m_Servant);

    if(!Check(tr))
        return false;

    boost::shared_ptr<AuthSampleObject> data = boost::shared_ptr<AuthSampleObject>(new AuthSampleObject());
    data->Owner(this);
    Setup(data);
    if( !data->Init(tr) ) return false;

    data->PosIP = in.SenderIP;
    data->PosPort = in.SenderPort;
    Add(data);
    LOG_PRINT_L4("ADD: "<<in.PaymentID<<"  in: "<<m_DAPIServer->Port());
    return true;
}

bool supernode::AuthSample::WalletProxyPay(const rpc_command::WALLET_PROXY_PAY::request& in, rpc_command::WALLET_PROXY_PAY::response& out, epee::json_rpc::error &er)
{
    boost::shared_ptr<BaseRTAObject> ff = ObjectByPayment(in.PaymentID);
    boost::shared_ptr<AuthSampleObject> data = boost::dynamic_pointer_cast<AuthSampleObject>(ff);
    if (!data)
    {
        LOG_PRINT_L4("not found object: "<<in.PaymentID<<"  in: "<<m_DAPIServer->Port());
        return false;
    }
    if (!data->WalletProxyPay(in, out, er))
    {
        LOG_PRINT_L4("!WalletProxyPay");
        Remove(data);
        return false;
    }
    return true;
}


bool supernode::AuthSample::Check(RTA_TransactionRecord& tr)
{
    // TODO: IMPL check sample selection if ok for givved block number
    return true;
}
