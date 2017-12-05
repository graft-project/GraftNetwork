#ifndef BASE_AUTH_OBJECT_H_
#define BASE_AUTH_OBJECT_H_

#include "supernode_rpc_command.h"
#include "SubNetBroadcast.h"
#include "DAPI_RPC_Server.h"
#include "FSN_Servant.h"
#include "DAPI_RPC_Client.h"
#include <string>
using namespace std;

namespace supernode {

	class BaseRTAObject {
		public:
		RTA_TransactionRecord TransactionRecord;

		public:
		virtual bool Init(const RTA_TransactionRecordBase& src);
		virtual void Set(const FSN_Servant* ser, DAPI_RPC_Server* dapi);
		virtual ~BaseRTAObject();

		protected:
		void InitSubnet();
		bool BroadcastRecord(const string& call);

		template<class IN_t, class OUT_t>
		bool SendDAPICall(const string& ip, const string& port, const string& method, IN_t& req, OUT_t& resp) {
			DAPI_RPC_Client call;
			call.Set(ip, port);
			req.PaymentID = TransactionRecord.PaymentID;
			return call.Invoke(method, req, resp);
		}

		bool CheckSign(const string& wallet, const string& sign);

		protected:
		SubNetBroadcast m_SubNetBroadcast;
		const FSN_Servant* m_Servant = nullptr;
		DAPI_RPC_Server* m_DAPIServer = nullptr;

	};

}

#endif /* BASE_AUTH_OBJECT_H_ */
