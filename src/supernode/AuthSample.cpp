#include "AuthSample.h"


void supernode::AuthSample::Init()  {
	m_DAPIServer->ADD_DAPI_HANDLER(PosProxySale, rpc_command::POS_PROXY_SALE, AuthSample);
	m_DAPIServer->ADD_DAPI_HANDLER(WalletProxyPay, rpc_command::WALLET_PROXY_PAY, AuthSample);
}


bool supernode::AuthSample::PosProxySale(const rpc_command::POS_PROXY_SALE::request& in, rpc_command::POS_PROXY_SALE::response& out) {
	RTA_TransactionRecord tr;
	rpc_command::ConvertToTR(tr, in, m_Servant);
	LOG_PRINT_L5("=OK-0");
	if( !Check(tr) ) return false;

	LOG_PRINT_L5("=OK-1");

	boost::shared_ptr<AuthSampleObject> data = boost::shared_ptr<AuthSampleObject>( new AuthSampleObject() );
	Setup(data);
	if( !data->Init(tr) ) return false;

	data->PosIP = in.SenderIP;
	data->PosPort = in.SenderPort;

	Add(data);

	LOG_PRINT_L5("=OK-2");


	return true;
}

bool supernode::AuthSample::WalletProxyPay(const rpc_command::WALLET_PROXY_PAY::request& in, rpc_command::WALLET_PROXY_PAY::response& out) {
	boost::shared_ptr<BaseRTAObject> ff = ObjectByPayment(in.PaymentID);
	boost::shared_ptr<AuthSampleObject> data = boost::dynamic_pointer_cast<AuthSampleObject>(ff);
	if(!data) { LOG_PRINT_L5("not found object"); return false; }


	RTA_TransactionRecord tr;
	rpc_command::ConvertToTR(tr, in, m_Servant);
	if( !data->WalletProxyPay(tr, out) ) { LOG_PRINT_L5("!WalletProxyPay"); RemoveRecord(data); return false; }


	return true;
}


bool supernode::AuthSample::Check(RTA_TransactionRecord& tr) {
	// TODO: IMPL check sample selection if ok for givved block number
	return true;
}



void supernode::AuthSample::RemoveRecord(boost::shared_ptr<AuthSampleObject> record) {}










