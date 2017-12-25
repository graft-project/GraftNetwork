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
	    		LOG_PRINT_L5("Failed to invoke http request to  " << call<<"  URI: "<<m_URI);
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

		protected:
		string m_URI;


	};


}

#endif /* DAPI_RPC_CLIENT_H_ */
