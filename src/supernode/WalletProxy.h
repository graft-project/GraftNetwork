#ifndef WALLET_PROXY_H_
#define WALLET_PROXY_H_

#include "WalletPayObject.h"
#include "baseclientproxy.h"

class WalletProxyTest_SendTx_Test;


namespace supernode {
    class WalletProxy : public BaseClientProxy {
		public:
		bool Pay(const rpc_command::WALLET_PAY::request& in, rpc_command::WALLET_PAY::response& out);
		bool WalletGetPosData(const rpc_command::WALLET_GET_POS_DATA::request& in, rpc_command::WALLET_GET_POS_DATA::response& out);
		bool WalletRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out);
        protected:
        void Init() override;
        friend class ::WalletProxyTest_SendTx_Test;

	};
}

#endif /* WALLET_PROXY_H_ */
