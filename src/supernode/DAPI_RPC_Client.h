#ifndef DAPI_RPC_CLIENT_H_
#define DAPI_RPC_CLIENT_H_

#include "net/http_base.h"
#include "net/http_client.h"
#include "net/jsonrpc_structs.h"
#include "storages/portable_storage_template_helper.h"
#include "storages/portable_storage.h"
#include "supernode_rpc_command.h"
#include <string>
using namespace std;


namespace supernode {

	class DAPI_RPC_Client : public epee::net_utils::http::http_simple_client {
		public:

		void Set(string ip, string port);

		template<class t_request, class t_response>
		bool Invoke(const string& call, const t_request& out_struct, t_response& result_struct, std::chrono::milliseconds timeout = std::chrono::seconds(5)) {

	    	epee::json_rpc::request<t_request> req;
	    	req.params = out_struct;
	    	req.method = call;
	    	req.jsonrpc = "2.0";

	    	std::string req_param;
	    	if(!epee::serialization::store_t_to_json(req, req_param)) return false;


	    	//req_param = string("{\"method\":\"")+call+("\",\"params\":\"")+req_param+("\"}");

	    	const epee::net_utils::http::http_response_info* pri = NULL;

	    	if(!invoke(rpc_command::DAPI_URI, rpc_command::DAPI_METHOD, req_param, timeout, std::addressof(pri))) {
	    		LOG_PRINT_L5("Failed to invoke http request to  " << call);
	    		return false;
	    	}

	    	if(!pri) {
	    		LOG_PRINT_L5("Failed to invoke http request to  " << call << ", internal error (null response ptr)");
	    		return false;
	    	}

	    	if(pri->m_response_code != 200) {
	    		LOG_PRINT_L5("Failed to invoke http request to  " << call << ", wrong response code: " << pri->m_response_code);
	    		return false;
	    	}

	    	epee::json_rpc::response<t_response, epee::json_rpc::dummy_error> resp;

	    	epee::serialization::portable_storage ps;
	    	if( !ps.load_from_json(pri->m_body) ) return false;
	    	if( !resp.load(ps) ) return false;

	    	result_struct = resp.result;

	    	return true;


		}


	};


}

#endif /* DAPI_RPC_CLIENT_H_ */
