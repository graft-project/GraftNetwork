#ifndef P2PBROADCAST_H_
#define P2PBROADCAST_H_

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <string>
#include <vector>
#include "SubNetBroadcast.h"
using namespace std;

namespace supernode {

	class P2P_Broadcast {
	public:
		// initial setup ip:port for bind for,
		// threadsNum - number of worker thread for send/recv data
		// seeds - neighbors from config, ip, port
		//void Set(const string& ip, const string& port, int threadsNum, const vector< pair<string, string> >& seeds );

		void Set(DAPI_RPC_Server* pa, const vector<string>& seeds );

		vector< pair<string, string> > Seeds();


		void Start();// start accept connection, NOT blocked call
		void Stop();//stop all worker threads and wait for it's end (join)



		// if we can;t use method as string and need to use int, so create class enum p2p_command : int {} in
		template<class IN_t>
		void AddHandler( const string& method, boost::function<void (const IN_t&)> handler ) {
			m_DAPIServer->Add_UUID_MethodHandler<IN_t, rpc_command::P2P_DUMMY_RESP>("p2p", method, [handler](const IN_t& in, rpc_command::P2P_DUMMY_RESP& out){
				handler(in);
				return true;
			}
			);
		}

		template<class IN_t, class OUT_t>
		void AddNearHandler( const string& method, boost::function<void (const IN_t&, OUT_t&)> handler ) {
			m_DAPIServer->Add_UUID_MethodHandler<IN_t, OUT_t>("p2p", method, [handler](const IN_t& in, OUT_t& out) {
				handler(in, out);
				return true;
			});
		}

		template<class IN_t, class OUT_t>
		bool SendNear( const string& method, IN_t& data, vector<OUT_t>& outv ) {
			data.PaymentID = "p2p";
			return m_SubNet.Send(method, data, outv);
		}


		// block until send to neighbors. return false if can't send
		template<class IN_t>
		bool Send( const string& method, IN_t& data ) {
			vector<rpc_command::P2P_DUMMY_RESP> out;
			data.PaymentID = "p2p";
			return m_SubNet.Send(method, data, out);
		}

	protected:
		SubNetBroadcast m_SubNet;
		DAPI_RPC_Server* m_DAPIServer = nullptr;




	};

} /* namespace supernode */

#endif /* P2PBROADCAST_H_ */
