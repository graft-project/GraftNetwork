#include "healthcheckapi.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/http_abstract_invoke.h"
#include "net/jsonrpc_structs.h"
#include "net/net_utils_base.h"

static const std::string HEALTH_URI("/health");

struct HealthResponse {
    string NodeAccess;

    BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(NodeAccess)
    END_KV_SERIALIZE_MAP()

};

supernode::HealthcheckAPI::HealthcheckAPI(const string &daemonAddress)
{
    boost::optional<epee::net_utils::http::login> login{};
    m_http_client.set_server("http://" + daemonAddress, login);
}

bool supernode::HealthcheckAPI::processHealthchecks(const string uri, epee::net_utils::http::http_response_info &response_info)
{
    if (uri == HEALTH_URI)
    {
        HealthResponse response;
        response.NodeAccess = cryptonodeCheck() ? "OK" : "Fail";
        epee::serialization::store_t_to_json(response, response_info.m_body);
        response_info.m_header_info.m_content_type = " application/json";
        response_info.m_mime_tipe = "application/json";
        response_info.m_response_comment = "OK";
        response_info.m_response_code = 200;
        return true;
    }
    return false;
}

bool supernode::HealthcheckAPI::cryptonodeCheck()
{
    epee::json_rpc::request<cryptonote::COMMAND_RPC_GET_VERSION::request> req_t = AUTO_VAL_INIT(req_t);
    epee::json_rpc::response<cryptonote::COMMAND_RPC_GET_VERSION::response, std::string> resp_t = AUTO_VAL_INIT(resp_t);
    req_t.jsonrpc = "2.0";
    req_t.id = epee::serialization::storage_entry(0);
    req_t.method = "get_version";
    bool r = epee::net_utils::invoke_http_json("/json_rpc", req_t, resp_t, m_http_client);
    if (r && resp_t.result.status == CORE_RPC_STATUS_OK)
    {
        return true;
    }
    return false;
}
