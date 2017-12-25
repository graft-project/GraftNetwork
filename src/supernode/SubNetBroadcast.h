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
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <string>
#include "DAPI_RPC_Client.h"
#include "DAPI_RPC_Server.h"
using namespace std;

namespace supernode {

	class DAPI_RPC_Server;

	class SubNetBroadcast {
		public:
		virtual ~SubNetBroadcast();
		// all messages will send with subnet_id
		// and handler will recieve only messages with subnet_id
		// as subnet_id we use PaymentID, generated as UUID in PoS
		// ALL data structs (IN and OUT) must be child form sub_net_data
		void Set( DAPI_RPC_Server* pa, string subnet_id, const vector< boost::shared_ptr<FSN_Data> >& members );
		void Set( DAPI_RPC_Server* pa, string subnet_id, const vector<string>& members );

		vector< pair<string, string> > Members();//port, ip

		public:
		template<class IN_t, class OUT_t>
		bool Send( const string& method, const IN_t& in, vector<OUT_t>& out ) {
			//LOG_PRINT_L5("SEND. size: "<<m_Members.size());
			boost::thread_group workers;
			out.resize( m_Members.size() );
			vector<int> rets;
			rets.resize( m_Members.size(), 0 );

			for(unsigned i=0;i<m_Members.size();i++) {
				workers.create_thread( boost::bind(&SubNetBroadcast::DoCallInThread<IN_t, OUT_t>, *this, method, in, &out[i], &rets[i], m_Members[i].first, m_Members[i].second) );
			}
			workers.join_all();

			bool ret = true;
			for(unsigned i=0;i<m_Members.size();i++) if( rets[i]==0 ) {
				ret = false; break;
			}


			if(!ret) out.clear();
			return ret;
		}

		template<class IN_t, class OUT_t>
		void AddHandler( const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {
			int idx = m_DAPIServer->Add_UUID_MethodHandler<IN_t, OUT_t>( m_PaymentID, method, handler );
			m_MyHandlers.push_back(idx);
		}
		#define ADD_SUBNET_HANDLER(method, data, class_owner) AddHandler<data::request, data::response>( dapi_call::method, bind( &class_owner::method, this, _1, _2) );

		protected:
		template<class IN_t, class OUT_t>
		void DoCallInThread(const string& method, const IN_t& in, OUT_t* outo, int* ret, const string& ip, const string& port) {
			//LOG_PRINT_L5("call to: "<<ip<<"  : "<<port<<"/"<<method<<"  from: "<<m_DAPIServer->Port());
			bool localcOk = false;
			for(unsigned k=0;k<4;k++) {
				DAPI_RPC_Client client;
				client.Set( ip, port );
				if( !client.Invoke<IN_t, OUT_t>(method, in, *outo, chrono::seconds(5)) ) {
					boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
					continue;
				}
				localcOk = true;
				break;
			}//for K
			*ret = localcOk?1:0;
		}//do work


		protected:
		DAPI_RPC_Server* m_DAPIServer = nullptr;
		vector< pair<string, string> > m_Members;//port, ip
		//vector< boost::shared_ptr<FSN_Data> > m_Members;
		string m_PaymentID;
		vector<int> m_MyHandlers;

};


}

#endif /* SUB_NET_BROADCAST_H_ */
