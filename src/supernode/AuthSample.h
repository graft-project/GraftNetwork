#ifndef AUTH_SAMPLE_H_
#define AUTH_SAMPLE_H_

#include "BaseRTAProcessor.h"
#include "AuthSampleObject.h"
#include "supernode_rpc_command.h"
#include <boost/pointer_cast.hpp>

namespace supernode {
	class AuthSample : public BaseRTAProcessor {
		public:



		protected:
		void Init() override;

		bool PosProxySale(const rpc_command::RTA_TRANSACTION_OBJECT::request& in, rpc_command::RTA_TRANSACTION_OBJECT::response& out);
		bool WalletProxyPay(const rpc_command::RTA_TRANSACTION_OBJECT::request& in, rpc_command::RTA_TRANSACTION_OBJECT::response& out);
		bool Check(RTA_TransactionRecord& tr);
		void RemoveRecord(boost::shared_ptr<AuthSampleObject> record);


	};


};




#endif /* AUTH_SAMPLE_H_ */
