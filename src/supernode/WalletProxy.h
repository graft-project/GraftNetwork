#ifndef WALLET_PROXY_H_
#define WALLET_PROXY_H_

#include "WalletPayObject.h"
#include "baseclientproxy.h"

namespace supernode {
    class WalletProxy : public BaseClientProxy {
		public:

		protected:
		void Init() override;

		bool Pay(const rpc_command::WALLET_PAY::request& in, rpc_command::WALLET_PAY::response& out);
		bool WalletGetPosData(const rpc_command::WALLET_GET_POS_DATA::request& in, rpc_command::WALLET_GET_POS_DATA::response& out);
	};
}

#endif /* WALLET_PROXY_H_ */
