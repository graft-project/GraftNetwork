#ifndef BASE_RTA_PROCESSOR_H_
#define BASE_RTA_PROCESSOR_H_

#include "BaseRTAObject.h"

namespace supernode {

	class BaseRTAProcessor {
		public:
		virtual ~BaseRTAProcessor();

		virtual void Start();
		virtual void Stop();

		void Set(const FSN_ServantBase* ser, DAPI_RPC_Server* dapi);

		protected:
		void Add(boost::shared_ptr<BaseRTAObject> obj);
		void Setup(boost::shared_ptr<BaseRTAObject> obj);
		boost::shared_ptr<BaseRTAObject> ObjectByPayment(const string& payment_id);

        virtual void Init() = 0;

		protected:
		const FSN_ServantBase* m_Servant = nullptr;
		DAPI_RPC_Server* m_DAPIServer = nullptr;
		mutable boost::mutex m_ObjectsGuard;
		vector< boost::shared_ptr<BaseRTAObject> > m_Objects;

	};

}

#endif /* BASE_RTA_PROCESSOR_H_ */
