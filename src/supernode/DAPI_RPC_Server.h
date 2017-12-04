#ifndef DAPI_RPC_SERVER_H_
#define DAPI_RPC_SERVER_H_

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "supernode_rpc_command.h"
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include "net/http_server_impl_base.h"


namespace supernode {

	class DAPI_RPC_Server : public epee::http_server_impl_base<DAPI_RPC_Server> {
		public:
		void Set(const string& ip, const string& port, int numThreads);
		void Start();

		public:
		template<class IN_t, class OUT_t>
		void AddHandler( const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {}
		#define ADD_DAPI_HANDLER(method, data, class_owner) AddHandler<data::request, data::response>( #method, bind( &class_owner::method, this, _1, _2) );
		#define ADD_SUBNET_BROADCAST_HANDLER(method, data, class_owner) AddHandler<data, rpc_command::SUB_NET_BROADCAST_RESPONCE>( dapi_call::method, bind( &class_owner::method, this, _1, _2) );

		// IN must be child from sub_net_data
		// income message filtered by payment_id and method
		template<class IN_t, class OUT_t>
		void Add_UUID_MethodHandler( uuid_t paymentid, const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {}
		#define ADD_DAPI_GLOBAL_METHOD_HANDLER(payid, method, data, class_owner) Add_UUID_MethodHandler<data::request, data::response>( payid, dapi_call::method, bind( &class_owner::method, this, _1, _2) );

		void RemoveHandler();// TODO: select handler identifyer and put it to call

		public:
		typedef epee::net_utils::connection_context_base connection_context;
		CHAIN_HTTP_TO_MAP2(connection_context)
		BEGIN_URI_MAP2()
			BEGIN_JSON_RPC_MAP("/dapi")
			END_JSON_RPC_MAP()
		END_URI_MAP2()



	};

};




#endif /* DAPI_RPC_SERVER_H_ */
