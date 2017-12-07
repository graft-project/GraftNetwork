#include "WalletPayObject.h"

bool supernode::WalletPayObject::Init(const RTA_TransactionRecordBase& src) {
	bool ret = _Init(src);
	m_Status = ret?NTRansactionStatus::Success:NTRansactionStatus::Fail;
	return ret;
}

bool supernode::WalletPayObject::_Init(const RTA_TransactionRecordBase& src) {
	BaseRTAObject::Init(src);

	m_DAPIServer->ADD_DAPI_GLOBAL_METHOD_HANDLER(TransactionRecord.PaymentID, GetPayStatus, rpc_command::WALLET_GET_TRANSACTION_STATUS, WalletPayObject);


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

	return true;
}

bool supernode::WalletPayObject::GetPayStatus(const rpc_command::WALLET_GET_TRANSACTION_STATUS::request& in, rpc_command::WALLET_GET_TRANSACTION_STATUS::response& out) {
	out.Status = int(m_Status);
	return true;
}


bool supernode::WalletPayObject::PutTXToPool() {
	// TODO: IMPL
	return true;
}


