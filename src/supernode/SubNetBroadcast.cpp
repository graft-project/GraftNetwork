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

static const unsigned s_MaxNotAvailCount = 4;

supernode::SubNetBroadcast::SubNetBroadcast(unsigned workerThreads) : m_Work(m_IOService) {
	for(unsigned i=0;i<workerThreads;i++) {
		m_Threadpool.create_thread( boost::bind(&boost::asio::io_service::run, &m_IOService) );
	}

}

supernode::SubNetBroadcast::~SubNetBroadcast() {
	for(auto a : m_MyHandlers) m_DAPIServer->RemoveHandler(a);
	m_MyHandlers.clear();

	m_IOService.stop();
	m_Threadpool.join_all();
}

vector< pair<string, string> > supernode::SubNetBroadcast::Members() {
	vector< pair<string, string> > ret;
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_MembersGuard);
		for(auto& a : m_Members) ret.push_back( make_pair(a.IP, a.Port) );
	}
	return ret;
}

void supernode::SubNetBroadcast::AddMember(const string& ip, const string& port) {
	boost::lock_guard<boost::recursive_mutex> lock(m_MembersGuard);
	_AddMember(ip, port);
}

void supernode::SubNetBroadcast::_AddMember(const string& ip, const string& port) {
	if( !AllowSendSefl && ip==m_DAPIServer->IP() && port==m_DAPIServer->Port() ) return;
	for(auto& a : m_Members) if( a.IP==ip && a.Port==port ) return;
	m_Members.push_back( SMember(ip, port) );
}

void supernode::SubNetBroadcast::Set( DAPI_RPC_Server* pa, string subnet_id, const vector< boost::shared_ptr<FSN_Data> >& members ) {
	m_DAPIServer = pa;
	m_PaymentID = subnet_id;
	boost::lock_guard<boost::recursive_mutex> lock(m_MembersGuard);
	for(auto a : members) _AddMember(a->IP, a->Port);
}

void supernode::SubNetBroadcast::Set( DAPI_RPC_Server* pa, string subnet_id, const vector<string>& members ) {
	m_DAPIServer = pa;
	m_PaymentID = subnet_id;
	boost::lock_guard<boost::recursive_mutex> lock(m_MembersGuard);
	for(auto a : members) {
		vector<string> vv = helpers::StrTok(a, ":");
		_AddMember( vv[0], vv[1] );
	}
}

void supernode::SubNetBroadcast::IncNoConnectAndRemove(const string& ip, const string& port) {
//	LOG_PRINT_L5("IncNoConnectAndRemove =1 : "<<m_DAPIServer->Port()<<"  REM: "<<port);
	boost::lock_guard<boost::recursive_mutex> lock(m_MembersGuard);
	for(unsigned i=0;i<m_Members.size();i++) if( m_Members[i].IP==ip && m_Members[i].Port==port ) {
		m_Members[i].NotAvailCount++;
//		LOG_PRINT_L5("IncNoConnectAndRemove =2 : "<<m_DAPIServer->Port()<<"  REM: "<<port);
		if( m_Members[i].NotAvailCount>=s_MaxNotAvailCount ) {
			m_Members.erase( m_Members.begin()+i );
//			LOG_PRINT_L5("IncNoConnectAndRemove =3 : "<<m_DAPIServer->Port()<<"  REM: "<<port);
		}
		break;
	}


}




