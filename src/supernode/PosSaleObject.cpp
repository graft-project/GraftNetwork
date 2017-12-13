#include "PosSaleObject.h"
#include "graft_defines.h"
#include <uuid/uuid.h>

void supernode::PosSaleObject::Owner(PosProxy* o) { m_Owner = o; }

bool supernode::PosSaleObject::Init(const RTA_TransactionRecordBase& src) {
	BaseRTAObject::Init(src);

	TransactionRecord.PaymentID = GeneratePaymentID();
	TransactionRecord.BlockNum = m_Servant->GetCurrentBlockHeight();
	TransactionRecord.AuthNodes = m_Servant->GetAuthSample( TransactionRecord.BlockNum );
	if( TransactionRecord.AuthNodes.empty() ) { LOG_PRINT_L5("SALE: AuthNodes.empty"); return false; }

	InitSubnet();

	vector<rpc_command::POS_PROXY_SALE::response> outv;
	rpc_command::POS_PROXY_SALE::request inbr;
	rpc_command::ConvertFromTR(inbr, TransactionRecord);
	inbr.SenderIP = m_DAPIServer->IP();
	inbr.SenderPort = m_DAPIServer->Port();
	if( !m_SubNetBroadcast.Send(dapi_call::PosProxySale, inbr, outv) || outv.empty() ) { return false; }

    m_Status = NTransactionStatus::InProgress;


	ADD_RTA_OBJECT_HANDLER(GetSaleStatus, rpc_command::POS_GET_SALE_STATUS, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(PoSTRSigned, rpc_command::POS_TR_SIGNED, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(PosRejectSale, rpc_command::POS_REJECT_SALE, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(AuthWalletRejectPay, rpc_command::WALLET_REJECT_PAY, PosSaleObject);

	return true;
}

bool supernode::PosSaleObject::AuthWalletRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out) {
	m_Status = NTransactionStatus::RejectedByWallet;
	return true;
}

bool supernode::PosSaleObject::GetSaleStatus(const rpc_command::POS_GET_SALE_STATUS::request& in, rpc_command::POS_GET_SALE_STATUS::response& out)
{
	out.Status = int(m_Status);
    out.Result = STATUS_OK;
	return true;
}


bool supernode::PosSaleObject::PoSTRSigned(const rpc_command::POS_TR_SIGNED::request& in, rpc_command::POS_TR_SIGNED::response& out) {
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_TxInPoolGotGuard);
		if(m_TxInPoolGot) return true;
		m_TxInPoolGot = true;
	}

	// TODO: get tranaction from pool by in.TransactionPoolID
	// TODO: check all signs

	/*
	if( !CheckSign(in.FSN_StakeWalletAddr, in.Sign) ) return false;
	m_Signs++;
	if( m_Signs!=m_Servant->AuthSampleSize() ) return true;// not all signs gotted
*/
    m_Status = NTransactionStatus::Success;
    return true;
}


bool supernode::PosSaleObject::PosRejectSale(const supernode::rpc_command::POS_REJECT_SALE::request &in, supernode::rpc_command::POS_REJECT_SALE::response &out) {
    m_Status = NTransactionStatus::Fail;

    //TODO: Add impl

    out.Result = STATUS_OK;
    return true;
}


string supernode::PosSaleObject::GeneratePaymentID() {
	uuid_t out;
	uuid_generate_time_safe(out);
	char uuid_str[37];
	uuid_unparse_lower(out, uuid_str);
	return uuid_str;
}

