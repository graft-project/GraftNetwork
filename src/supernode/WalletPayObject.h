#ifndef WALLET_PAY_OBJECT_H_
#define WALLET_PAY_OBJECT_H_

#include "BaseRTAObject.h"
#include "graft_wallet.h"

#include <memory>

namespace supernode {

	class WalletProxy;

	class WalletPayObject : public BaseRTAObject {
		public:
		void Owner(WalletProxy* o);
        /*!
         * @brief OpenSourceWallet - opens temporary wallet
         * @param wallet           - serialized wallet keys
         * @param walletPass       - key's password?
         * @return                 - true if success
         */
        bool OpenSenderWallet(const std::string &wallet, const std::string &walletPass);
        /*!
         * \brief Init - actually sends tx to tx pool
         * \param src - transaction details
         * \return    - true if tx sent sucessfully
         */
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
        std::unique_ptr<tools::GraftWallet> m_wallet;


	};

}

#endif /* WALLET_PAY_OBJECT_H_ */
