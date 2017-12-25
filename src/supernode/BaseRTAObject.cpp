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


#include "BaseRTAObject.h"

supernode::BaseRTAObject::BaseRTAObject() {
	TimeMark = boost::posix_time::second_clock::local_time();
}

bool supernode::BaseRTAObject::Init(const RTA_TransactionRecordBase& src) {
	RTA_TransactionRecordBase& dst = TransactionRecord;
	dst = src;
	return true;
}

void supernode::BaseRTAObject::Set(const FSN_ServantBase* ser, DAPI_RPC_Server* dapi) {
	m_Servant = ser;
	m_DAPIServer = dapi;
}

supernode::BaseRTAObject::~BaseRTAObject() {}

void supernode::BaseRTAObject::InitSubnet() {
	m_SubNetBroadcast.Set(m_DAPIServer, TransactionRecord.PaymentID, TransactionRecord.AuthNodes);
}


bool supernode::BaseRTAObject::CheckSign(const string& wallet, const string& sign) {
	return m_Servant->IsSignValid( TransactionRecord.MessageForSign(), wallet, sign );
}

void supernode::BaseRTAObject::MarkForDelete() {
	boost::lock_guard<boost::recursive_mutex> lock(m_HanlderIdxGuard);
	m_ReadyForDelete = true;
	for(auto a : m_HanlderIdx) m_DAPIServer->RemoveHandler(a);
	m_HanlderIdx.clear();
}

