#ifndef WALLET_PAY_OBJECT_H_
#define WALLET_PAY_OBJECT_H_

#include "BaseRTAObject.h"


namespace supernode {

	class WalletProxy;

	class WalletPayObject : public BaseRTAObject {
		public:
		void Owner(WalletProxy* o);
		bool Init(const RTA_TransactionRecordBase& src) override;

		bool GetPayStatus(const rpc_command::WALLET_GET_TRANSACTION_STATUS::request& in, rpc_command::WALLET_GET_TRANSACTION_STATUS::response& out);


		protected:
		virtual bool PutTXToPool();
		bool _Init(const RTA_TransactionRecordBase& src);

		protected:
        NTransactionStatus m_Status = NTransactionStatus::None;
        WalletProxy* m_Owner = nullptr;
        vector<string> m_Signs;
        string m_TransactionPoolID;


	};

}

#endif /* WALLET_PAY_OBJECT_H_ */
