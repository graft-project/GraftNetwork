#ifndef WALLET_PROXY_H_
#define WALLET_PROXY_H_

#include "WalletPayObject.h"
#include "BaseRTAProcessor.h"

namespace supernode {
	class WalletProxy : public BaseRTAProcessor {
		public:


		protected:
		void Init() override;

		bool Pay(const rpc_command::WALLET_PAY::request& in, rpc_command::WALLET_PAY::response& out);

	};
}

#endif /* WALLET_PROXY_H_ */
