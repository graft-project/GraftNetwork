#ifndef POS_SALE_OBJECT_H_
#define POS_SALE_OBJECT_H_

#include "BaseRTAObject.h"

namespace supernode {

	class PosSaleObject : public BaseRTAObject {
		public:
		bool Init(const RTA_TransactionRecordBase& src) override;
		bool GetSaleStatus(const rpc_command::POS_GET_SALE_STATUS::request& in, rpc_command::POS_GET_SALE_STATUS::response& out);
		bool PoSTRSigned(const rpc_command::POS_TR_SIGNED::request& in, rpc_command::POS_TR_SIGNED::response& out);


		protected:
		int m_Signs = 0;


	};

}

#endif /* POS_SALE_OBJECT_H_ */
