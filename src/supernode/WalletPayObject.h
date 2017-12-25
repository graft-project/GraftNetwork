// Copyright (c) 2017, The Graft Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

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
