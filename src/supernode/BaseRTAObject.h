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

#ifndef BASE_AUTH_OBJECT_H_
#define BASE_AUTH_OBJECT_H_

#include "supernode_rpc_command.h"
#include "SubNetBroadcast.h"
#include "DAPI_RPC_Server.h"
#include "FSN_ServantBase.h"
#include "DAPI_RPC_Client.h"
#include <string>
using namespace std;

namespace supernode {

	class BaseRTAObject {
		public:
		RTA_TransactionRecord TransactionRecord;

		public:
		BaseRTAObject();
		virtual bool Init(const RTA_TransactionRecordBase& src);
		virtual void Set(const FSN_ServantBase* ser, DAPI_RPC_Server* dapi);
		virtual ~BaseRTAObject();
		void MarkForDelete();

		boost::posix_time::ptime TimeMark;



		protected:
		void InitSubnet();

		template<class IN_t, class OUT_t>
		bool SendDAPICall(const string& ip, const string& port, const string& method, IN_t& req, OUT_t& resp) {
			DAPI_RPC_Client call;
			call.Set(ip, port);
			req.PaymentID = TransactionRecord.PaymentID;
			return call.Invoke(method, req, resp);
		}

		bool CheckSign(const string& wallet, const string& sign);

        template<class IN_t, class OUT_t, class ERR_t>
        void AddHandler( const string& method, boost::function<bool (const IN_t&, OUT_t&, ERR_t&)> handler ) {
			boost::lock_guard<boost::recursive_mutex> lock(m_HanlderIdxGuard);
			if(m_ReadyForDelete) return;
            int idx = m_DAPIServer->Add_UUID_MethodHandler<IN_t, OUT_t, ERR_t>( TransactionRecord.PaymentID, method, handler );
			m_HanlderIdx.push_back(idx);
		}


		#define ADD_RTA_OBJECT_HANDLER(method, data, class_owner) \
            AddHandler<data::request, data::response, epee::json_rpc::error>(dapi_call::method, bind( &class_owner::method, this, _1, _2, _3))


		protected:
		SubNetBroadcast m_SubNetBroadcast;
		const FSN_ServantBase* m_Servant = nullptr;
		DAPI_RPC_Server* m_DAPIServer = nullptr;

		mutable boost::recursive_mutex m_HanlderIdxGuard;
		vector<int> m_HanlderIdx;
		bool m_ReadyForDelete = false;

	};

}

#endif /* BASE_AUTH_OBJECT_H_ */
