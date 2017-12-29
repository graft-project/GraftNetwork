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

#include "supernode_rpc_command.h"
#include "FSN_ServantBase.h"

const string supernode::rpc_command::DAPI_URI = "/dapi";
const string supernode::rpc_command::DAPI_METHOD = "POST";
const string supernode::rpc_command::DAPI_PROTOCOL = "http";

#define DCALL(xx) const string supernode::dapi_call::xx = #xx;
DCALL(Pay);
DCALL(GetPayStatus);
DCALL(Sale);
DCALL(PosRejectSale)
DCALL(GetSaleStatus);
DCALL(WalletProxyPay);
DCALL(WalletTRSigned);
DCALL(WalletPutTxInPool);
DCALL(PosProxySale);
DCALL(PoSTRSigned);
DCALL(WalletGetPosData);
DCALL(WalletProxyGetPosData);
DCALL(GetWalletBalance)
DCALL(CreateAccount)
DCALL(GetSeed)
DCALL(RestoreAccount)
DCALL(WalletRejectPay)
DCALL(WalletProxyRejectPay)
DCALL(AuthWalletRejectPay)
DCALL(FSN_CheckWalletOwnership);
#undef DCALL

#define P2P_CALL(xx) const string supernode::p2p_call::xx = #xx;
P2P_CALL(AddFSN);
P2P_CALL(LostFSNStatus);
P2P_CALL(GetFSNList);
P2P_CALL(AddSeed);
P2P_CALL(GetSeedsList);
#undef P2P_CALL


void supernode::rpc_command::ConvertFromTR(RTA_TransactionRecordRequest& in_dst, const RTA_TransactionRecord& in_src) {
	RTA_TransactionRecordBase& dstb = in_dst;
	const RTA_TransactionRecordBase& srcb = in_src;
	dstb = srcb;

	for(auto a : in_src.AuthNodes) in_dst.NodesWallet.push_back( a->Stake.Addr );
}

void supernode::rpc_command::ConvertToTR(RTA_TransactionRecord& in_dst, const RTA_TransactionRecordRequest& in_src, const FSN_ServantBase* servant) {
	RTA_TransactionRecordBase& dstb = in_dst;
	const RTA_TransactionRecordBase& srcb = in_src;
	dstb = srcb;

	for(auto a : in_src.NodesWallet) {
		boost::shared_ptr<FSN_Data> node = servant->FSN_DataByStakeAddr(a);
		if(!node) continue;
		in_dst.AuthNodes.push_back( node );
	}

}
