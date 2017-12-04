#ifndef P2PBROADCAST_H_
#define P2PBROADCAST_H_

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <string>
#include <vector>
using namespace std;

namespace supernode {

	class P2P_Broadcast {
	public:
		// initial setup ip:port for bind for,
		// threadsNum - number of worker thread for send/recv data
		// seeds - neighbors from config, ip, port
		void Set(const string& ip, const string& port, int threadsNum, const vector< pair<string, string> >& seeds );
		void Start();// start accept connection, NOT blocked call

		// if we can;t use method as string and need to use int, so create class enum p2p_command : int {} in
		template<class IN_t>
		void AddHandler( const string& method, boost::function<bool (const IN_t&)> handler ) {}

		// block until send to neighbors. return false if can't send
		template<class IN_t>
		bool Send( const string& method, const IN_t& data ) { return false; }




	};

} /* namespace supernode */

#endif /* P2PBROADCAST_H_ */
