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

#include <string>
using namespace std;
#include "DAPI_RPC_Server.h"

bool supernode::DAPI_RPC_Server::handle_http_request(const epee::net_utils::http::http_request_info& query_info, epee::net_utils::http::http_response_info& response, connection_context& m_conn_context) {
	//LOG_PRINT_L4("HTTP [" << m_conn_context.m_remote_address.host_str() << "] " << query_info.m_http_method_str << " " << query_info.m_URI);

	//LOG_PRINT_L5("Got request");

	//LOG_PRINT_L5("req in "<<Port()<<"  =1");

	response.m_response_code = 200;
	response.m_response_comment = "Ok";

	response.m_additional_fields.push_back( make_pair("Access-Control-Allow-Origin", "*") );
	response.m_additional_fields.push_back( make_pair("Access-Control-Allow-Credentials", "true") );
	response.m_additional_fields.push_back( make_pair("Access-Control-Allow-Methods", "GET, PUT, POST, DELETE, OPTIONS") );
	response.m_additional_fields.push_back( make_pair("Access-Control-Max-Age", "1728000") );
	response.m_additional_fields.push_back( make_pair("Access-Control-Allow-Headers", "X-Requested-With, Content-Type, Origin, Cache-Control, Pragma, Authorization, Accept, Accept-Encoding") );

	DAPICallResult ret = HandleRequest(query_info, response, m_conn_context);
	if( ret!="" ) {
		response.m_response_code = 500;
		response.m_response_comment = ret;;
	}



	//LOG_PRINT_L5("req in "<<Port()<<"  =2");

	return true;
}

supernode::DAPICallResult supernode::DAPI_RPC_Server::HandleRequest(const epee::net_utils::http::http_request_info& query_info, epee::net_utils::http::http_response_info& response_info, connection_context& m_conn_context) {
	if( query_info.m_URI!=rpc_command::DAPI_URI ) return "not DAPI URI";

    uint64_t ticks = epee::misc_utils::get_tick_count();
    epee::serialization::portable_storage ps;

//    epee::json_rpc::error_response rsp;
    string version;
    std::string callback_name;
    epee::serialization::storage_entry id_ = epee::serialization::storage_entry(std::string());

    if( !ps.load_from_json(query_info.m_body) ) {
		return "Parse error: can't load_from_json";
    } else if( !ps.get_value("dapi_version", version, nullptr) ) {
     	return "No DAPI version";
    } else if( !ps.get_value("method", callback_name, nullptr) ) {
    	return "No method";
    } else if( version!=rpc_command::DAPI_VERSION ) {
    	return "Wrong DAPI version";
    }


    ps.get_value("id", id_, nullptr);

    std::string payment_id;
    {
    	epee::json_rpc::request<SubNetData> resp;
    	if( resp.load(ps) ) payment_id = resp.params.PaymentID;
    }



    SCallHandler* handler = nullptr;

    {
    	//LOG_PRINT_L5("\n\n\n");

    	boost::lock_guard<boost::recursive_mutex> lock(m_Handlers_Guard);
    	for(unsigned i=0;i<m_vHandlers.size();i++) {
    		SHandlerData& hh = m_vHandlers[i];

    		//LOG_PRINT_L5("Have: '"<<hh.Name<<"' : '"<<hh.PaymentID<<"' need: '"<<callback_name<<"' : '"<<payment_id<<"'");

    		if(hh.Name!=callback_name) continue;
    		if(hh.PaymentID.size() &&  hh.PaymentID!=payment_id) continue;

    		handler = hh.Handler;
    		break;
    	}
    	//LOG_PRINT_L5("\n\n\n");
    }

    if(!handler) { return string("No handler not found for: ")+callback_name; }
    //LOG_PRINT_L5("Befor process: "<<callback_name<<"  in: "<<Port());
    DAPICallResult ret = handler->Process(ps, response_info.m_body);
    if( ret!="" ) { return ret; }
    //LOG_PRINT_L5("After process: "<<callback_name<<"  in: "<<Port());

    response_info.m_mime_tipe = "application/json";
    response_info.m_header_info.m_content_type = " application/json";
    return "";
}

const string& supernode::DAPI_RPC_Server::IP() const { return m_IP; }
const string& supernode::DAPI_RPC_Server::Port() const { return m_Port; }


void supernode::DAPI_RPC_Server::Set(const string& ip, const string& port, int numThreads) {
	m_Port = port;
	m_IP = ip;
	init(port, ip);
	m_NumThreads = numThreads;

	//m_net_server.acceptor_.max_connections
	//LOG_PRINT_L5("MAX_CON: "<<boost::asio::ip::tcp::acceptor::max_connections);

}

void supernode::DAPI_RPC_Server::Start() { run(m_NumThreads); }

void supernode::DAPI_RPC_Server::Stop() { send_stop_signal(); }

int supernode::DAPI_RPC_Server::AddHandlerData(const SHandlerData& h) {
	boost::lock_guard<boost::recursive_mutex> lock(m_Handlers_Guard);
	int idx = m_HandlerIdx;
	m_HandlerIdx++;
	m_vHandlers.push_back(h);
	m_vHandlers.rbegin()->Idx = idx;
	return idx;
}

void supernode::DAPI_RPC_Server::RemoveHandler(int idx) {
	boost::lock_guard<boost::recursive_mutex> lock(m_Handlers_Guard);
	for(unsigned i=0;i<m_vHandlers.size();i++) if( m_vHandlers[i].Idx==idx ) {
		m_vHandlers.erase( m_vHandlers.begin()+i );
		break;
	}
}

