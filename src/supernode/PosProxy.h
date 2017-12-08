#ifndef POS_PROXY_H_
#define POS_PROXY_H_

#include "PosSaleObject.h"
#include "baseclientproxy.h"

namespace supernode {
    class PosProxy : public BaseClientProxy {
		public:

		protected:
		void Init() override;

		bool Sale(const rpc_command::POS_SALE::request& in, rpc_command::POS_SALE::response& out);
	};
}

#endif /* POS_PROXY_H_ */
