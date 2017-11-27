#include "http_rpc_client.h"

void http_rpc_client::set(string ip, string port, string protocol, string uri, const string& method) {
	string ss = protocol+string("://")+ip+string(":")+port;
	boost::optional<epee::net_utils::http::login> http_login{};
	set_server(ss, http_login);

	m_uri = uri;
	m_method = method;
}

