#include "PosSaleObject.h"

bool supernode::PosSaleObject::Init(const RTA_TransactionRecordBase& src) {
	BaseRTAObject::Init(src);

	TransactionRecord.BlockNum = m_Servant->GetCurrentBlockHeight();
	TransactionRecord.AuthNodes = m_Servant->GetAuthSample( TransactionRecord.BlockNum );
	if( TransactionRecord.AuthNodes.empty() ) return false;

	InitSubnet();
	if( !BroadcastRecord(dapi_call::PosProxySale) ) return false;

	// TODO: add all other handlers for this sale request
	m_DAPIServer->ADD_DAPI_GLOBAL_METHOD_HANDLER(TransactionRecord.PaymentID, GetSaleStatus, rpc_command::POS_GET_SALE_STATUS, PosSaleObject);
	m_DAPIServer->ADD_DAPI_GLOBAL_METHOD_HANDLER(TransactionRecord.PaymentID, PoSTRSigned, rpc_command::POS_TR_SIGNED, PosSaleObject);

	return true;
}

bool supernode::PosSaleObject::GetSaleStatus(const rpc_command::POS_GET_SALE_STATUS::request& in, rpc_command::POS_GET_SALE_STATUS::response& out) {
	// TODO: IMPL
	return true;
}


bool supernode::PosSaleObject::PoSTRSigned(const rpc_command::POS_TR_SIGNED::request& in, rpc_command::POS_TR_SIGNED::response& out) {
	if( !CheckSign(in.FSN_StakeWalletAddr, in.Sign) ) return false;
	m_Signs++;
	if( m_Signs!=m_Servant->AuthSampleSize() ) return true;// not all signs gotted

	// TODO: set transaction state to OK
	return true;
}




