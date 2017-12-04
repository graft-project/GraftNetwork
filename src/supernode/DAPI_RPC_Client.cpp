#include "DAPI_RPC_Client.h"

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

void supernode::DAPI_RPC_Client::Set(string ip, string port) {
	string ss = rpc_command::DAPI_PROTOCOL+string("://")+ip+string(":")+port;
	boost::optional<epee::net_utils::http::login> http_login{};
	set_server(ss, http_login);
}
