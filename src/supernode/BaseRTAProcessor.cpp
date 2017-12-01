#include "BaseRTAProcessor.h"


supernode::BaseRTAProcessor::~BaseRTAProcessor() {}

void supernode::BaseRTAProcessor::Start() {}
void supernode::BaseRTAProcessor::Stop() {}

void supernode::BaseRTAProcessor::Set(const FSN_Servant* ser, DAPI_RPC_Server* dapi) {
	m_Servant = ser;
	m_DAPIServer = dapi;
	Init();
}

void supernode::BaseRTAProcessor::Add(boost::shared_ptr<BaseRTAObject> obj) {
	boost::lock_guard<boost::mutex> lock(m_ObjectsGuard);
	m_Objects.push_back(obj);
}

void supernode::BaseRTAProcessor::Setup(boost::shared_ptr<BaseRTAObject> obj) {
	obj->Set(m_Servant, m_DAPIServer);
}

boost::shared_ptr<supernode::BaseRTAObject> supernode::BaseRTAProcessor::ObjectByPayment(const uuid_t payment_id) {
	boost::shared_ptr<BaseRTAObject> ret;
	{
		boost::lock_guard<boost::mutex> lock(m_ObjectsGuard);
		for(auto& a : m_Objects) if(a->TransactionRecord.PaymentID==payment_id) {
			ret = a;
			break;
		}
	}
	return ret;
}

