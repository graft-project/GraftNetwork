#ifndef DAPI_RPC_SERVER_H_
#define DAPI_RPC_SERVER_H_

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "supernode_rpc_command.h"

namespace supernode {

	class DAPI_RPC_Server {
		public:
		void Set(const string& ip, const string& port, int numThreads);
		void Start();

		public:
		template<class IN_t, class OUT_t>
		void AddHandler( const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {}
		#define ADD_DAPI_HANDLER(method, data, class_owner) AddHandler<data::request, data::response>( #method, bind( &class_owner::method, this, _1, _2) );
		#define ADD_SUBNET_BROADCAST_HANDLER(method, data, class_owner) AddHandler<data, rpc_command::SUB_NET_BROADCAST_RESPONCE>( #method, bind( &class_owner::method, this, _1, _2) );

		// IN must be child from sub_net_data
		// income message filtered by payment_id and method
		template<class IN_t, class OUT_t>
		void Add_UUID_MethodHandler( uuid_t paymentid, const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {}
		#define ADD_DAPI_GLOBAL_METHOD_HANDLER(payid, method, data, class_owner) Add_UUID_MethodHandler<data::request, data::response>( payid, #method, bind( &class_owner::method, this, _1, _2) );
	};

};




#endif /* DAPI_RPC_SERVER_H_ */
