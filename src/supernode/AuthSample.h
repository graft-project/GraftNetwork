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

		bool PosProxySale(const rpc_command::POS_PROXY_SALE::request& in, rpc_command::POS_PROXY_SALE::response& out);
		bool WalletProxyPay(const rpc_command::WALLET_PROXY_PAY::request& in, rpc_command::WALLET_PROXY_PAY::response& out);
		bool Check(RTA_TransactionRecord& tr);

	};


};




#endif /* AUTH_SAMPLE_H_ */
