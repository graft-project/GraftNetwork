#include "WalletPayObject.h"
#include "graft_defines.h"

bool supernode::WalletPayObject::Init(const RTA_TransactionRecordBase& src) {
	bool ret = _Init(src);
    m_Status = ret ? NTransactionStatus::Success : NTransactionStatus::Fail;
	return ret;
}

bool supernode::WalletPayObject::_Init(const RTA_TransactionRecordBase& src) {
	BaseRTAObject::Init(src);

	// we allready have block num
	TransactionRecord.AuthNodes = m_Servant->GetAuthSample( TransactionRecord.BlockNum );
	if( TransactionRecord.AuthNodes.empty() ) return false;

	InitSubnet();

	vector<rpc_command::WALLET_PROXY_PAY::response> outv;
	rpc_command::WALLET_PROXY_PAY::request inbr;
	rpc_command::ConvertFromTR(inbr, TransactionRecord);
	if( !m_SubNetBroadcast.Send(dapi_call::WalletProxyPay, inbr, outv) || outv.empty() ) return false;


	if( outv.size()!=m_Servant->AuthSampleSize() ) return false;// not all signs gotted
	for(auto& a : outv) {
		if( !CheckSign(a.FSN_StakeWalletAddr, a.Sign) ) return false;
	}

	// m_Signs.push_back(in); ??

	if( !PutTXToPool() ) return false;

	rpc_command::WALLET_PUT_TX_IN_POOL::request req;
	req.PaymentID = TransactionRecord.PaymentID;
	for(auto& a : outv) {
		req.FSN_Wallets.push_back( a.FSN_StakeWalletAddr );
		req.Signs.push_back( a.Sign );
	}

	vector<rpc_command::WALLET_PUT_TX_IN_POOL::response> vv_out;

	if( !m_SubNetBroadcast.Send( dapi_call::WalletPutTxInPool, req, vv_out) ) return false;


	ADD_RTA_OBJECT_HANDLER(GetPayStatus, rpc_command::WALLET_GET_TRANSACTION_STATUS, WalletPayObject);
	ADD_RTA_OBJECT_HANDLER(RejectPay, rpc_command::WALLET_REJECT_PAY, WalletPayObject);
	//m_DAPIServer->ADD_DAPI_GLOBAL_METHOD_HANDLER(TransactionRecord.PaymentID, GetPayStatus, rpc_command::WALLET_GET_TRANSACTION_STATUS, WalletPayObject);


	return true;
}

bool supernode::WalletPayObject::GetPayStatus(const rpc_command::WALLET_GET_TRANSACTION_STATUS::request& in, rpc_command::WALLET_GET_TRANSACTION_STATUS::response& out) {
	out.Status = int(m_Status);
    return true;
}

bool supernode::WalletPayObject::RejectPay(const supernode::rpc_command::WALLET_REJECT_PAY::request &in, supernode::rpc_command::WALLET_REJECT_PAY::response &out)
{
    m_Status = NTransactionStatus::Fail;

    //TODO: Add impl

    out.Result = STATUS_OK;
    return true;
}


bool supernode::WalletPayObject::PutTXToPool() {
	// TODO: IMPL
	return true;
}


