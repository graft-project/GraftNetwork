#ifndef DAPI_RPC_SERVER_H_
#define DAPI_RPC_SERVER_H_

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "supernode_rpc_command.h"
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include "net/http_server_impl_base.h"
#include <string>
using namespace std;

namespace supernode {

	class DAPI_RPC_Server : public epee::http_server_impl_base<DAPI_RPC_Server> {
		public:
		typedef epee::net_utils::connection_context_base connection_context;

		public:
		void Set(const string& ip, const string& port, int numThreads);
		void Start();//block
		void Stop();
		const string& IP() const;
		const string& Port() const;

		protected:
		class SCallHandler {
			public:
			virtual bool Process(epee::serialization::portable_storage& in, string& out_js)=0;
		};
		template<class IN_t, class OUT_t>
		class STemplateHandler : public SCallHandler {
			public:
			STemplateHandler( boost::function<bool (const IN_t&, OUT_t&)>& handler) : Handler(handler) {}

			bool Process(epee::serialization::portable_storage& ps, string& out_js) {
				  boost::value_initialized<epee::json_rpc::request<IN_t> > req_;
				  epee::json_rpc::request<IN_t>& req = static_cast<epee::json_rpc::request<IN_t>&>(req_);
				  if( !req.load(ps) ) return false;

				  boost::value_initialized<epee::json_rpc::response<OUT_t, epee::json_rpc::dummy_error> > resp_;
				  epee::json_rpc::response<OUT_t, epee::json_rpc::dummy_error>& resp =  static_cast<epee::json_rpc::response<OUT_t, epee::json_rpc::dummy_error> &>(resp_);
				  resp.jsonrpc = "2.0";
				  resp.id = req.id;

				  if( !Handler(req.params, resp.result) ) return false;

				  epee::serialization::store_t_to_json(resp, out_js);
				  return true;
			}

			boost::function<bool (const IN_t&, OUT_t&)> Handler;
		};

		struct SHandlerData {
			SCallHandler* Handler = nullptr;
			string Name;
			int Idx = -1;
			string PaymentID;
		};


		public:
		template<class IN_t, class OUT_t>
		int AddHandler( const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {
			SHandlerData hh;
			hh.Handler = new STemplateHandler<IN_t, OUT_t>(handler);
			hh.Name = method;
			return AddHandlerData(hh);
		}

		#define ADD_DAPI_HANDLER(method, data, class_owner) AddHandler<data::request, data::response>( #method, bind( &class_owner::method, this, _1, _2) )

		// IN must be child from sub_net_data
		// income message filtered by payment_id and method
		template<class IN_t, class OUT_t>
		int Add_UUID_MethodHandler( string paymentid, const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {
			SHandlerData hh;
			hh.Handler = new STemplateHandler<IN_t, OUT_t>(handler);
			hh.Name = method;
			hh.PaymentID = paymentid;
			return AddHandlerData(hh);
		}

		#define ADD_DAPI_GLOBAL_METHOD_HANDLER(payid, method, data, class_owner) Add_UUID_MethodHandler<data::request, data::response>( payid, dapi_call::method, bind( &class_owner::method, this, _1, _2) )

		void RemoveHandler(int idx);


		protected:
		bool handle_http_request(const epee::net_utils::http::http_request_info& query_info, epee::net_utils::http::http_response_info& response, connection_context& m_conn_context) override;
		bool HandleRequest(const epee::net_utils::http::http_request_info& query_info, epee::net_utils::http::http_response_info& response_info, connection_context& m_conn_context);
		int AddHandlerData(const SHandlerData& h);

		protected:
		boost::recursive_mutex m_Handlers_Guard;
		vector<SHandlerData> m_vHandlers;
		int m_HandlerIdx = 0;

		protected:
		int m_NumThreads = 5;

		protected:
		string m_IP;
		string m_Port;


	};

};




#endif /* DAPI_RPC_SERVER_H_ */
