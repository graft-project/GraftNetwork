#include "BaseRTAObject.h"

supernode::BaseRTAObject::BaseRTAObject() {
	TimeMark = boost::posix_time::second_clock::local_time();
}

bool supernode::BaseRTAObject::Init(const RTA_TransactionRecordBase& src) {
	RTA_TransactionRecordBase& dst = TransactionRecord;
	dst = src;
	return true;
}

void supernode::BaseRTAObject::Set(const FSN_ServantBase* ser, DAPI_RPC_Server* dapi) {
	m_Servant = ser;
	m_DAPIServer = dapi;
}

supernode::BaseRTAObject::~BaseRTAObject() {}

void supernode::BaseRTAObject::InitSubnet() {
	m_SubNetBroadcast.Set(m_DAPIServer, TransactionRecord.PaymentID, TransactionRecord.AuthNodes);
}


bool supernode::BaseRTAObject::CheckSign(const string& wallet, const string& sign) {
	m_Servant->IsSignValid( TransactionRecord.MessageForSign(), wallet, sign );
	return true;
}

void supernode::BaseRTAObject::MarkForDelete() {
	boost::lock_guard<boost::recursive_mutex> lock(m_HanlderIdxGuard);
	m_ReadyForDelete = true;
	for(auto a : m_HanlderIdx) m_DAPIServer->RemoveHandler(a);
	m_HanlderIdx.clear();
}

