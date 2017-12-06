#include "supernode_rpc_command.h"
#include "FSN_ServantBase.h"

const string supernode::rpc_command::DAPI_URI = "/dapi";
const string supernode::rpc_command::DAPI_METHOD = "GET";
const string supernode::rpc_command::DAPI_PROTOCOL = "http";

#define DCALL(xx) const string supernode::dapi_call::xx = #xx;
DCALL(Pay);
DCALL(GetPayStatus);
DCALL(Sale);
DCALL(GetSaleStatus);
DCALL(WalletProxyPay);
DCALL(WalletGetPosData);
DCALL(WalletTRSigned);
DCALL(WalletPutTxInPool);
DCALL(PosProxySale);
DCALL(PoSTRSigned);
#undef DCALL


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
