#include "BaseRTAObject.h"



bool supernode::BaseRTAObject::Init(const RTA_TransactionRecordBase& src) {
	RTA_TransactionRecordBase& dst = TransactionRecord;
	dst = src;
	return true;
}

void supernode::BaseRTAObject::Set(const FSN_Servant* ser, DAPI_RPC_Server* dapi) {
	m_Servant = ser;
	m_DAPIServer = dapi;
}

supernode::BaseRTAObject::~BaseRTAObject() {}

void supernode::BaseRTAObject::InitSubnet() {
	m_SubNetBroadcast.Set(m_DAPIServer, TransactionRecord.PaymentID, TransactionRecord.AuthNodes);
}

bool supernode::BaseRTAObject::BroadcastRecord(const string& call) {
	return m_SubNetBroadcast.Send(call, TransactionRecord);
}

bool supernode::BaseRTAObject::CheckSign(const string& wallet, const string& sign) {
	// TODO: IMPL. We need sign string?
	return true;
}
