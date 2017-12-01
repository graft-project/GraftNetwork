#ifndef SUB_NET_BROADCAST_H_
#define SUB_NET_BROADCAST_H_

#include "supernode_common_struct.h"
#include <boost/function.hpp>
#include <boost/bind.hpp>


namespace supernode {

	class DAPI_RPC_Server;

	class SubNetBroadcast {
		public:
		// all messages will send with subnet_id
		// and handler will recieve only messages with subnet_id
		// as subnet_id we use PaymentID, generated as UUID in PoS
		// ALL data structs (IN and OUT) must be child form sub_net_data
		void Set( DAPI_RPC_Server* pa, uuid_t subnet_id, const vector< boost::shared_ptr<FSN_Data> >& members );



		public:
		template<class T>
		bool Send( const string& method, const T& data ) { return true; }// return false if any of call's to other was fail

		template<class T>
		void AddHandler( const string& method, boost::function<void (T)> handler ) {}

		protected:
		DAPI_RPC_Server* m_DAPIServer = nullptr;

};


}

#endif /* SUB_NET_BROADCAST_H_ */
