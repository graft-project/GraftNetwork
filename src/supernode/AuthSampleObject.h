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

#ifndef AUTH_SAMPLE_OBJECT_H_
#define AUTH_SAMPLE_OBJECT_H_

#include "BaseRTAObject.h"
#include "supernode_common_struct.h"
#include "DAPI_RPC_Client.h"

namespace supernode {
	class AuthSample;
	class AuthSampleObject: public BaseRTAObject {
		public:
		void Owner(AuthSample* o);
		bool Init(const RTA_TransactionRecord& src);
        bool WalletProxyPay(const rpc_command::WALLET_PROXY_PAY::request& in, rpc_command::WALLET_PROXY_PAY::response& out, epee::json_rpc::error &er);

		public:
		string PosIP;
		string PosPort;

		protected:
        bool WalletPutTxInPool(const rpc_command::WALLET_PUT_TX_IN_POOL::request& in, rpc_command::WALLET_PUT_TX_IN_POOL::response& out, epee::json_rpc::error &er);
        bool WalletProxyGetPosData(const rpc_command::WALLET_GET_POS_DATA::request& in, rpc_command::WALLET_GET_POS_DATA::response& out, epee::json_rpc::error &er);
        bool WalletProxyRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out, epee::json_rpc::error &er);

		protected:
		string GenerateSignForTransaction();

		protected:
		virtual bool Init(const RTA_TransactionRecordBase& src);

		protected:
		AuthSample* m_Owner = nullptr;


	};

}

#endif /* AUTH_SAMPLE_OBJECT_H_ */
