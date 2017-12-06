#include "WalletProxy.h"

void supernode::WalletProxy::Init() {
	m_DAPIServer->ADD_DAPI_HANDLER(Pay, rpc_command::WALLET_PAY, WalletProxy);
	// TODO: add all other handlers
}


bool supernode::WalletProxy::Pay(const rpc_command::WALLET_PAY::request& in, rpc_command::WALLET_PAY::response& out) {
	boost::shared_ptr<WalletPayObject> data = boost::shared_ptr<WalletPayObject>( new WalletPayObject() );
	Setup(data);
	if( !data->Init(in) ) return false;
	Add(data);

	out.DataForClientWallet = data->TransactionRecord.DataForClientWallet;
	// TODO: return vector<sign>

	return true;
}
