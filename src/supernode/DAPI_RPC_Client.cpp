#include "DAPI_RPC_Client.h"


void supernode::DAPI_RPC_Client::Set(string ip, string port) {
	string ss = rpc_command::DAPI_PROTOCOL+string("://")+ip+string(":")+port;
	boost::optional<epee::net_utils::http::login> http_login{};
	set_server(ss, http_login);
}
