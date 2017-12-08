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
		virtual bool Init(const RTA_TransactionRecordBase& src);
		virtual void Set(const FSN_ServantBase* ser, DAPI_RPC_Server* dapi);
		virtual void RemoveAllHandlers();
		virtual ~BaseRTAObject();

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

		template<class IN_t, class OUT_t>
		void AddHandler( const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {
			boost::lock_guard<boost::mutex> lock(m_HanlderIdxGuard);
			if(m_ReadyForDelete) return;
			int idx = m_DAPIServer->Add_UUID_MethodHandler<IN_t, OUT_t>( TransactionRecord.PaymentID, method, handler );
			m_HanlderIdx.push_back(idx);
		}


		#define ADD_RTA_OBJECT_HANDLER(method, data, class_owner) \
			AddHandler<data::request, data::response>(dapi_call::method, bind( &class_owner::method, this, _1, _2))



		protected:
		SubNetBroadcast m_SubNetBroadcast;
		const FSN_ServantBase* m_Servant = nullptr;
		DAPI_RPC_Server* m_DAPIServer = nullptr;

		mutable boost::mutex m_HanlderIdxGuard;
		vector<int> m_HanlderIdx;
		bool m_ReadyForDelete = false;

	};

}

#endif /* BASE_AUTH_OBJECT_H_ */
