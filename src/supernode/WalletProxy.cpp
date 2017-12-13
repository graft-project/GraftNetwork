#include "WalletProxy.h"

void supernode::WalletProxy::Init()
{
    BaseClientProxy::Init();
	m_DAPIServer->ADD_DAPI_HANDLER(Pay, rpc_command::WALLET_PAY, WalletProxy);
	m_DAPIServer->ADD_DAPI_HANDLER(WalletGetPosData, rpc_command::WALLET_GET_POS_DATA, WalletProxy);
	m_DAPIServer->ADD_DAPI_HANDLER(WalletRejectPay, rpc_command::WALLET_REJECT_PAY, WalletProxy);
}

bool supernode::WalletProxy::WalletRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out) {
	// TODO: if have PayID, don't call

	SubNetBroadcast sub;
	sub.Set( m_DAPIServer, in.PaymentID, m_Servant->GetAuthSample(in.BlockNum) );
	vector<rpc_command::WALLET_REJECT_PAY::response> vout;
	bool ret = sub.Send( dapi_call::WalletProxyRejectPay, in, vout );
	return ret;
}

bool supernode::WalletProxy::Pay(const rpc_command::WALLET_PAY::request& in, rpc_command::WALLET_PAY::response& out)
{
	boost::shared_ptr<WalletPayObject> data = boost::shared_ptr<WalletPayObject>( new WalletPayObject() );
	data->Owner(this);
	Setup(data);
    if (!data->Init(in))
    {
        return false;
    }
	Add(data);

    std::unique_ptr<tools::GraftWallet> wal = initWallet(in.Account, in.Password);
    supernode::GraftTxExtra graft_extra;
    // TODO: fill graft extra fields

    bool result = false;
//    PendingTransaction *transaction = wal->createTransaction(in.POSAddress, in.PaymentID, in.Amount, 0, graft_extra);
//    LOG_PRINT_L2("About to send  tx: " << transaction->txid() << ", amount: " << cryptonote::print_money(transaction->amount()));
//    bool result = transaction->commit();
//    if (!result) {
//        LOG_PRINT_L2("Error sending tx: " << transaction->errorString());
//    }


    return result;
}

bool supernode::WalletProxy::WalletGetPosData(const rpc_command::WALLET_GET_POS_DATA::request& in, rpc_command::WALLET_GET_POS_DATA::response& out) {

	// we allready have block num
	vector< boost::shared_ptr<FSN_Data> > vv = m_Servant->GetAuthSample( in.BlockNum );
	if( vv.size()!=m_Servant->AuthSampleSize() ) return false;

	boost::shared_ptr<FSN_Data> data = *vv.begin();

	DAPI_RPC_Client call;
	call.Set(data->IP, data->Port);
	return call.Invoke(dapi_call::WalletProxyGetPosData, in, out);
}
