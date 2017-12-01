#ifndef DAPI_RPC_CLIENT_H_
#define DAPI_RPC_CLIENT_H_

#include <string>
using namespace std;

namespace supernode {

	class DAPI_RPC_Client {
		public:

		void Set(string ip, string port);

		template<class t_request, class t_response>
		bool Invoke_HTTP_JSON(const string& call, const t_request& out_struct, t_response& result_struct) {
			return true;
		}
	};


}

#endif /* DAPI_RPC_CLIENT_H_ */
