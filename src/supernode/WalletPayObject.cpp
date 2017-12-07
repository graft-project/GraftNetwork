#include "WalletPayObject.h"

bool supernode::WalletPayObject::Init(const RTA_TransactionRecordBase& src) {
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


	TransactionRecord.DataForClientWallet = outv.begin()->DataForClientWallet;

	// TODO: check all signs HERE



	return true;
}

bool supernode::WalletPayObject::GetPayStatus(const rpc_command::WALLET_GET_TRANSACTION_STATUS::request& in, rpc_command::WALLET_GET_TRANSACTION_STATUS::response& out) {
	// TODO: IMPL
	return true;
}

/*
bool supernode::WalletPayObject::WalletTRSigned(const rpc_command::WALLET_TR_SIGNED::request& in, rpc_command::WALLET_TR_SIGNED::response& out) {
	if( !CheckSign(in.FSN_StakeWalletAddr, in.Sign) ) return false;

	m_Signs.push_back(in);
	if( m_Signs.size()!=m_Servant->AuthSampleSize() ) return true;// not all signs gotted

	if( !PutTXToPool() ) return false;

	rpc_command::WALLET_PUT_TX_IN_POOL::request req;
	req.PaymentID = TransactionRecord.PaymentID;
	for(auto& a : m_Signs) {
		req.FSN_Wallets.push_back( a.FSN_StakeWalletAddr );
		req.Signs.push_back( a.Sign );
	}

	vector<rpc_command::WALLET_PUT_TX_IN_POOL::response> vv_out;

	if( !m_SubNetBroadcast.Send( dapi_call::WalletPutTxInPool, req, vv_out) ) return false;

	// TODO: set status to SUCCESS


	return true;
}
*/

bool supernode::WalletPayObject::PutTXToPool() {
	// TODO: IMPL
	return true;
}


