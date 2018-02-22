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
#include "PosProxy.h"

void supernode::PosProxy::Init() {
    BaseClientProxy::Init();
    m_Work.Workers(10);
    m_DAPIServer->ADD_DAPI_HANDLER(Sale, rpc_command::POS_SALE, PosProxy);
	// TODO: add all other handlers
}


bool supernode::PosProxy::Sale(const rpc_command::POS_SALE::request& in, rpc_command::POS_SALE::response& out, epee::json_rpc::error &er) {
	LOG_PRINT_L0("PosProxy::Sale" << in.POSAddress << in.Amount);
    //TODO: Add input data validation
	boost::shared_ptr<PosSaleObject> data = boost::shared_ptr<PosSaleObject>( new PosSaleObject() );
	data->Owner(this);
	Setup(data);


    if (!data->Init(in))
    {
        out.Result = ERROR_SALE_REQUEST_FAILED;
        LOG_ERROR("ERROR_SALE_REQUEST_FAILED");
        return false;
    }
	Add(data);

	m_Work.Service.post( [data](){
		data->ContinueInit();
	} );

	out.BlockNum = data->TransactionRecord.BlockNum;
	out.PaymentID = data->TransactionRecord.PaymentID;
    out.Result = STATUS_OK;
	return true;
}
