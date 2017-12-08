#include "AuthSampleObject.h"

bool supernode::AuthSampleObject::Init(const RTA_TransactionRecord& src) {
	TransactionRecord = src;
	//m_DAPIServer->ADD_DAPI_GLOBAL_METHOD_HANDLER(TransactionRecord.PaymentID, WalletProxyGetPosData, rpc_command::WALLET_GET_POS_DATA, AuthSampleObject);
	ADD_RTA_OBJECT_HANDLER(WalletProxyGetPosData, rpc_command::WALLET_GET_POS_DATA, AuthSampleObject);
	return true;
}


bool supernode::AuthSampleObject::WalletProxyPay(const RTA_TransactionRecord& src, rpc_command::WALLET_PROXY_PAY::response& out) {
	if(src!=TransactionRecord) return false;

	// TODO: send LOCK. WTF?? all our nodes got this packet by sub-net broadcast. so only top node must send broad cast

	ADD_RTA_OBJECT_HANDLER(WalletPutTxInPool, rpc_command::WALLET_PUT_TX_IN_POOL, AuthSampleObject);
	//m_DAPIServer->ADD_DAPI_GLOBAL_METHOD_HANDLER(TransactionRecord.PaymentID, WalletPutTxInPool, rpc_command::WALLET_PUT_TX_IN_POOL, AuthSampleObject);

	out.Sign = GenerateSignForWallet();
	out.FSN_StakeWalletAddr = m_Servant->GetMyStakeWallet().Addr;

	return true;
}

bool supernode::AuthSampleObject::WalletPutTxInPool(const rpc_command::WALLET_PUT_TX_IN_POOL::request& in, rpc_command::WALLET_PUT_TX_IN_POOL::response& out) {
	// TODO: check all signs and fns wallet - this sign was generated for wallet!
	// TODO: check, if Tx realy send to pool

	// all ok, notify PoS about this
	rpc_command::POS_TR_SIGNED::request req;
	rpc_command::POS_TR_SIGNED::response resp;
	req.Sign = GenerateSignForPos();
	req.FSN_StakeWalletAddr = m_Servant->GetMyStakeWallet().Addr;
	if( !SendDAPICall(PosIP, PosPort, dapi_call::PoSTRSigned, req, resp) ) return false;


	return true;
}

bool supernode::AuthSampleObject::WalletProxyGetPosData(const rpc_command::WALLET_GET_POS_DATA::request& in, rpc_command::WALLET_GET_POS_DATA::response& out) {
	out.DataForClientWallet = TransactionRecord.DataForClientWallet;
	return true;
}

string supernode::AuthSampleObject::GenerateSignForWallet() {
	// TODO: IMPL
	return "";
}

string supernode::AuthSampleObject::GenerateSignForPos() {
	// TODO: IMPL
	return "";
}

bool supernode::AuthSampleObject::Init(const RTA_TransactionRecordBase& src) { return false; }




