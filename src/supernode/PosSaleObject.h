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

#ifndef POS_SALE_OBJECT_H_
#define POS_SALE_OBJECT_H_

#include "BaseRTAObject.h"

namespace supernode {
	class PosProxy;
    class TxPool;
	class PosSaleObject : public BaseRTAObject {
		public:
		void Owner(PosProxy* o);
		bool Init(const RTA_TransactionRecordBase& src) override;
		void ContinueInit();
        bool GetSaleStatus(const rpc_command::POS_GET_SALE_STATUS::request& in, rpc_command::POS_GET_SALE_STATUS::response& out, epee::json_rpc::error &er);
        bool PoSTRSigned(const rpc_command::POS_TR_SIGNED::request& in, rpc_command::POS_TR_SIGNED::response& out, epee::json_rpc::error &er);

        bool PosRejectSale(const rpc_command::POS_REJECT_SALE::request &in, rpc_command::POS_REJECT_SALE::response &out, epee::json_rpc::error &er);
        bool AuthWalletRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out, epee::json_rpc::error &er);

		protected:
		string GeneratePaymentID();

		protected:
        unsigned m_Signs = 0;
        NTransactionStatus m_Status = NTransactionStatus::None;
        PosProxy* m_Owner = nullptr;
        mutable boost::recursive_mutex m_TxInPoolGotGuard;
        bool m_TxInPoolGot = false;
	};
}

#endif /* POS_SALE_OBJECT_H_ */
