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

#include "SubNetBroadcast.h"
#include "supernode_helpers.h"

supernode::SubNetBroadcast::~SubNetBroadcast() {
	for(auto a : m_MyHandlers) m_DAPIServer->RemoveHandler(a);
	m_MyHandlers.clear();
}

vector< pair<string, string> > supernode::SubNetBroadcast::Members() { return m_Members; }

void supernode::SubNetBroadcast::Set( DAPI_RPC_Server* pa, string subnet_id, const vector< boost::shared_ptr<FSN_Data> >& members ) {
	m_DAPIServer = pa;
	m_PaymentID = subnet_id;
	for(auto a : members) m_Members.push_back( make_pair(a->IP, a->Port) );
}

void supernode::SubNetBroadcast::Set( DAPI_RPC_Server* pa, string subnet_id, const vector<string>& members ) {
	m_DAPIServer = pa;
	m_PaymentID = subnet_id;
	for(auto a : members) {
		vector<string> vv = helpers::StrTok(a, ":");
		if( vv[0]==pa->IP() && vv[1]==pa->Port() ) continue;
		m_Members.push_back( make_pair(vv[0], vv[1]) );
	}
}

