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

#ifndef SUB_NET_BROADCAST_H_
#define SUB_NET_BROADCAST_H_

#include "supernode_common_struct.h"
#include <string>
#include "DAPI_RPC_Client.h"
#include "DAPI_RPC_Server.h"
#include "WorkerPool.h"
#include "ddlock.h"
using namespace std;

namespace supernode {

	class DAPI_RPC_Server;

	class SubNetBroadcast {
		public:
		SubNetBroadcast(unsigned workerThreads=10);
		virtual ~SubNetBroadcast();
		// all messages will send with subnet_id
		// and handler will recieve only messages with subnet_id
		// as subnet_id we use PaymentID, generated as UUID in PoS
		// ALL data structs (IN and OUT) must be child form sub_net_data
		void Set( DAPI_RPC_Server* pa, string subnet_id, const vector< boost::shared_ptr<FSN_Data> >& members );
		void Set( DAPI_RPC_Server* pa, string subnet_id, const vector<string>& members );

		struct SMember {
			SMember(const string& ip, const string& p) { IP = ip; Port = p; }
			string IP;
			string Port;
			unsigned NotAvailCount = 0;
		};

		vector< pair<string, string> > Members();//port, ip
		void AddMember(const string& ip, const string& port);

		unsigned RetryCount = 2;
		std::chrono::milliseconds CallTimeout = std::chrono::seconds(5);
		bool AllowSendSefl = true;

		public:
		template<class IN_t, class OUT_t>
		bool Send( const string& method, const IN_t& in, vector<OUT_t>& out, bool reqAllResps=true ) {
			if(m_SendsCount) throw string("m_SendsCount not zero");

			m_MembersGuard.lock();

			out.resize( m_Members.size() );
			vector<int> rets;
			rets.resize( m_Members.size(), 0 );
			m_SendsCount = m_Members.size();


			for(unsigned i=0;i<m_Members.size();i++) {
				OUT_t* outp = &out[i];
				int* retp = &rets[i];
				string ip = m_Members[i].IP;
				string port = m_Members[i].Port;
				m_Work.Service.post(
					[this, method, in, outp, retp, ip, port]() {
					DoCallInThread<IN_t, OUT_t>(method, in, outp, retp, ip, port);
				} );
			}

			m_MembersGuard.unlock();

			while(m_SendsCount) boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

			bool ret = true;

			if(reqAllResps) {
				for(unsigned i=0;i<m_Members.size();i++) if( rets[i]==0 ) {
					ret = false; break;
				}
				if(!ret) out.clear();
			} else {
				 vector<OUT_t> vv;
				 for(unsigned i=0;i<out.size();i++) if( rets[i]!=0 ) vv.push_back( out[i] );
				 out = vv;
			}

			return ret;
		}

		template<class IN_t>
		void Send( const string& method, const IN_t& in) {
			if(m_SendsCount) throw string("m_SendsCount not zero");

			rpc_command::P2P_DUMMY_RESP* outp = &m_DummyOut;
			int* retp = &m_DummyRet;

            boost::lock_guard<supernode::graft_ddmutex> lock(m_MembersGuard);

			for(unsigned i=0;i<m_Members.size();i++) {
				string ip = m_Members[i].IP;
				string port = m_Members[i].Port;
				m_Work.Service.post(
					[this, method, in, outp, retp, ip, port]() {
					DoCallInThread<IN_t, rpc_command::P2P_DUMMY_RESP>(method, in, outp, retp, ip, port);
				} );
			}//for
		}


		template<class IN_t, class OUT_t>
		void AddHandler( const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {
			int idx = m_DAPIServer->Add_UUID_MethodHandler<IN_t, OUT_t>( m_PaymentID, method, handler );
			m_MyHandlers.push_back(idx);
		}
		#define ADD_SUBNET_HANDLER(method, data, class_owner) AddHandler<data::request, data::response>( dapi_call::method, bind( &class_owner::method, this, _1, _2) );

		public:
		template<class IN_t, class OUT_t>
		void DoCallInThread(string method, const IN_t in, OUT_t* outo, int* ret, string ip, string port) {
			bool localcOk = false;
			bool wasNoConnect = false;
			for(unsigned k=0;k<RetryCount;k++) {
				DAPI_RPC_Client client;
				client.Set( ip, port );
				if( !client.Invoke<IN_t, OUT_t>(method, in, *outo, CallTimeout) ) {
					wasNoConnect = wasNoConnect || !client.WasConnected;
					boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
					continue;
				}
				localcOk = true;
				break;
			}//for K
			*ret = localcOk?1:0;
			if(!localcOk && wasNoConnect) IncNoConnectAndRemove(ip, port);

			if(m_SendsCount) m_SendsCount--;
		}//do work


		protected:
		void _AddMember(const string& ip, const string& port);
		void IncNoConnectAndRemove(const string& ip, const string& port);

		protected:
		DAPI_RPC_Server* m_DAPIServer = nullptr;
        supernode::graft_ddmutex m_MembersGuard;
		vector<SMember> m_Members;
		string m_PaymentID;
		vector<int> m_MyHandlers;

		protected:
		int m_SendsCount = 0;
	    WorkerPool m_Work;

		protected:
		rpc_command::P2P_DUMMY_RESP m_DummyOut;
		int m_DummyRet=0;



};


}

#endif /* SUB_NET_BROADCAST_H_ */
