#include "DAPI_RPC_Server.h"

bool supernode::DAPI_RPC_Server::handle_http_request(const epee::net_utils::http::http_request_info& query_info, epee::net_utils::http::http_response_info& response, connection_context& m_conn_context) {
	LOG_PRINT_L5("HTTP [" << m_conn_context.m_remote_address.host_str() << "] " << query_info.m_http_method_str << " " << query_info.m_URI);

	response.m_response_code = 200;
	response.m_response_comment = "Ok";
	if( !HandleRequest(query_info, response, m_conn_context) ) {
		response.m_response_code = 500;
		response.m_response_comment = "Internal server error";
	}

	return true;
}

bool supernode::DAPI_RPC_Server::HandleRequest(const epee::net_utils::http::http_request_info& query_info, epee::net_utils::http::http_response_info& response_info, connection_context& m_conn_context) {
	if( query_info.m_URI!=rpc_command::DAPI_URI ) return false;

    uint64_t ticks = epee::misc_utils::get_tick_count();
    epee::serialization::portable_storage ps;
    if( !ps.load_from_json(query_info.m_body) ) {
       boost::value_initialized<epee::json_rpc::error_response> rsp;
       static_cast<epee::json_rpc::error_response&>(rsp).error.code = -32700;
       static_cast<epee::json_rpc::error_response&>(rsp).error.message = "Parse error";
       epee::serialization::store_t_to_json(static_cast<epee::json_rpc::error_response&>(rsp), response_info.m_body);
       return true;
    }

    epee::serialization::storage_entry id_;
    id_ = epee::serialization::storage_entry(std::string());
    ps.get_value("id", id_, nullptr);
    std::string callback_name;
    if( !ps.get_value("method", callback_name, nullptr) ) {
      epee::json_rpc::error_response rsp;
      rsp.jsonrpc = "2.0";
      rsp.error.code = -32600;
      rsp.error.message = "Invalid Request";
      epee::serialization::store_t_to_json(static_cast<epee::json_rpc::error_response&>(rsp), response_info.m_body);
      return true;
    }

    std::string payment_id;
    {
    	epee::json_rpc::request<SubNetData> resp;
    	if( resp.load(ps) ) payment_id = resp.params.PaymentID;
    }



    SCallHandler* handler = nullptr;

    {
    	boost::lock_guard<boost::mutex> lock(m_Handlers_Guard);
    	for(unsigned i=0;i<m_vHandlers.size();i++) {
    		SHandlerData& hh = m_vHandlers[i];
    		if(hh.Name!=callback_name) continue;
    		if(hh.PaymentID!=payment_id) continue;

    		handler = hh.Handler;
    		break;
    	}
    }

    if(!handler) return false;
    if( !handler->Process(ps, response_info.m_body) ) return false;

    response_info.m_mime_tipe = "application/json";
    response_info.m_header_info.m_content_type = " application/json";
    return true;
}

void supernode::DAPI_RPC_Server::Set(const string& ip, const string& port, int numThreads) {
	init(port, ip);
	m_NumThreads = numThreads;
}

void supernode::DAPI_RPC_Server::Start() { run(m_NumThreads); }

void supernode::DAPI_RPC_Server::Stop() { send_stop_signal(); }

int supernode::DAPI_RPC_Server::AddHandlerData(const SHandlerData& h) {
	boost::lock_guard<boost::mutex> lock(m_Handlers_Guard);
	int idx = m_HandlerIdx;
	m_HandlerIdx++;
	m_vHandlers.push_back(h);
	m_vHandlers.rbegin()->Idx = idx;
	return idx;
}

void supernode::DAPI_RPC_Server::RemoveHandler(int idx) {
	boost::lock_guard<boost::mutex> lock(m_Handlers_Guard);
	for(unsigned i=0;i<m_vHandlers.size();i++) if( m_vHandlers[i].Idx==idx ) {
		m_vHandlers.erase( m_vHandlers.begin()+i );
		break;
	}
}

