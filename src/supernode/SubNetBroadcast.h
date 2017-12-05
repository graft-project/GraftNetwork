#ifndef SUB_NET_BROADCAST_H_
#define SUB_NET_BROADCAST_H_

#include "supernode_common_struct.h"
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <string>
#include "DAPI_RPC_Client.h"
#include "DAPI_RPC_Server.h"
using namespace std;

namespace supernode {

	class DAPI_RPC_Server;

	class SubNetBroadcast {
		public:
		virtual ~SubNetBroadcast();
		// all messages will send with subnet_id
		// and handler will recieve only messages with subnet_id
		// as subnet_id we use PaymentID, generated as UUID in PoS
		// ALL data structs (IN and OUT) must be child form sub_net_data
		void Set( DAPI_RPC_Server* pa, string subnet_id, const vector< boost::shared_ptr<FSN_Data> >& members );



		public:
		template<class IN_t, class OUT_t>
		bool Send( const string& method, const IN_t& in, vector<OUT_t>& out ) {
			// TODO: refactor, use worker thread pool

			out.clear();
			bool ret = true;
			for(unsigned i=0;i<m_Members.size();i++) {
				DAPI_RPC_Client client;
				client.Set( m_Members[i]->IP, m_Members[i]->Port );
				OUT_t outo;
				if( !client.Invoke<IN_t, OUT_t>(method, in, outo) ) { ret = false; break; }
				out.push_back(outo);
			}

			if(!ret) { out.clear(); return false; }

			return true;
		}

		template<class IN_t, class OUT_t>
		void AddHandler( const string& method, boost::function<bool (const IN_t&, OUT_t&)> handler ) {
			int idx = m_DAPIServer->Add_UUID_MethodHandler<IN_t, OUT_t>( m_PaymentID, method, handler );
			m_MyHandlers.push_back(idx);
		}
		#define ADD_SUBNET_HANDLER(method, data, class_owner) AddHandler<data::request, data::response>( dapi_call::method, bind( &class_owner::method, this, _1, _2) );

		protected:
		DAPI_RPC_Server* m_DAPIServer = nullptr;
		vector< boost::shared_ptr<FSN_Data> > m_Members;
		string m_PaymentID;
		vector<int> m_MyHandlers;

};


}

#endif /* SUB_NET_BROADCAST_H_ */
