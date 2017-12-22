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
