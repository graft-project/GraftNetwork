#include "BaseRTAObject.h"



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

bool supernode::BaseRTAObject::BroadcastRecord(const string& call) {
	rpc_command::RTA_TRANSACTION_OBJECT::request in;
	rpc_command::ConvertFromTR(in, TransactionRecord);
	vector<rpc_command::RTA_TRANSACTION_OBJECT::response> out;
	return m_SubNetBroadcast.Send(call, in, out);
}

bool supernode::BaseRTAObject::CheckSign(const string& wallet, const string& sign) {
	// TODO: IMPL. We need sign string?
	return true;
}
