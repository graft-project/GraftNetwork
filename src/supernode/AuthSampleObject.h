#ifndef AUTH_SAMPLE_OBJECT_H_
#define AUTH_SAMPLE_OBJECT_H_

#include "BaseRTAObject.h"
#include "supernode_common_struct.h"
#include "DAPI_RPC_Client.h"

namespace supernode {
	class AuthSampleObject: public BaseRTAObject {
		public:
		bool Init(const RTA_TransactionRecord& src);
		bool WalletProxyPay(const RTA_TransactionRecord& src, rpc_command::WALLET_PROXY_PAY::response& out);

		public:
		string PosIP;
		string PosPort;

		protected:
		bool WalletPutTxInPool(const rpc_command::WALLET_PUT_TX_IN_POOL::request& in, rpc_command::WALLET_PUT_TX_IN_POOL::response& out);

		protected:
		string GenerateSignForWallet();
		string GenerateSignForPos();

		protected:
		virtual bool Init(const RTA_TransactionRecordBase& src);


	};

}

#endif /* AUTH_SAMPLE_OBJECT_H_ */
