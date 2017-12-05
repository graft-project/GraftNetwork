#include "WalletPayObject.h"

bool supernode::WalletPayObject::Init(const RTA_TransactionRecordBase& src) {
	BaseRTAObject::Init(src);

	// we allready have block num
	TransactionRecord.AuthNodes = m_Servant->GetAuthSample( TransactionRecord.BlockNum );

	InitSubnet();
	if( !BroadcastRecord( dapi_call::WalletProxyPay ) ) return false;

	auto dd = TransactionRecord.AuthNodes[ rand()*TransactionRecord.AuthNodes.size()/RAND_MAX ];
	rpc_command::WALLET_GET_POS_DATA::request req;
	rpc_command::WALLET_GET_POS_DATA::response resp;

	if( !SendDAPICall(dd->IP, dd->Port, dapi_call::WalletGetPosData, req, resp) ) return false;
	TransactionRecord.DataForClientWallet = resp.DataForClientWallet;

	// TODO: add all other handlers for this sale request
	m_DAPIServer->ADD_DAPI_GLOBAL_METHOD_HANDLER(TransactionRecord.PaymentID, GetPayStatus, rpc_command::WALLET_GET_TRANSACTION_STATUS, WalletPayObject);
	m_DAPIServer->ADD_DAPI_GLOBAL_METHOD_HANDLER(TransactionRecord.PaymentID, WalletTRSigned, rpc_command::WALLET_TR_SIGNED, WalletPayObject);

	return true;
}

bool supernode::WalletPayObject::GetPayStatus(const rpc_command::WALLET_GET_TRANSACTION_STATUS::request& in, rpc_command::WALLET_GET_TRANSACTION_STATUS::response& out) {
	// TODO: IMPL
	return true;
}

bool supernode::WalletPayObject::WalletTRSigned(const rpc_command::WALLET_TR_SIGNED::request& in, rpc_command::WALLET_TR_SIGNED::response& out) {
	if( !CheckSign(in.FSN_StakeWalletAddr, in.Sign) ) return false;

	m_Signs.push_back(in);
	if( m_Signs.size()!=FSN_Servant::FSN_PerAuthSample ) return true;// not all signs gotted

	if( !PutTXToPool() ) return false;

	rpc_command::WALLET_PUT_TX_IN_POOL::request req;
	req.PaymentID = TransactionRecord.PaymentID;
	for(auto& a : m_Signs) {
		req.FSN_Wallets.push_back( a.FSN_StakeWalletAddr );
		req.Signs.push_back( a.Sign );
	}

	if( !m_SubNetBroadcast.Send( dapi_call::WalletPutTxInPool, req) ) return false;

	// TODO: set status to SUCCESS


	return true;
}

bool supernode::WalletPayObject::PutTXToPool() {
	// TODO: IMPL
	return true;
}


