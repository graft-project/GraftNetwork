#include "WalletPayObject.h"
#include "graft_defines.h"
#include "WalletProxy.h"

void supernode::WalletPayObject::Owner(WalletProxy* o) { m_Owner = o; }

bool supernode::WalletPayObject::OpenSenderWallet(const string &wallet, const string &walletPass)
{
    // m_Owner is WalletProxy here
    m_wallet = m_Owner->initWallet(wallet, walletPass);
    if (!m_wallet) {
        LOG_ERROR("Error initializing wallet");
        return false;
    }
    m_wallet->refresh();
    return true;
}


bool supernode::WalletPayObject::Init(const RTA_TransactionRecordBase& src) {
	bool ret = _Init(src);
    m_Status = ret ? NTransactionStatus::Success : NTransactionStatus::Fail;
	return ret;
}

bool supernode::WalletPayObject::_Init(const RTA_TransactionRecordBase& src) {
	BaseRTAObject::Init(src);

	// we allready have block num
	TransactionRecord.AuthNodes = m_Servant->GetAuthSample( TransactionRecord.BlockNum );
    if ( TransactionRecord.AuthNodes.empty() ) {
        LOG_ERROR("Failed to get auth sample");
        return false;
    }

	InitSubnet();

	vector<rpc_command::WALLET_PROXY_PAY::response> outv;
	rpc_command::WALLET_PROXY_PAY::request inbr;
	rpc_command::ConvertFromTR(inbr, TransactionRecord);
    if( !m_SubNetBroadcast.Send(dapi_call::WalletProxyPay, inbr, outv) || outv.empty() )  {
        LOG_ERROR("Failed to send WalletProxyPay broadcast");
        return false;
    }

    LOG_PRINT_L2("obtained " << outv.size() << " WALLET_PROXY_PAY responses");
    if (outv.size() == 0) {
        LOG_ERROR("NO WALLET_PROXY_PAY responses obtained");
        return false;
    }

    if (outv.size() != m_Servant->AuthSampleSize()) {
        LOG_ERROR("outv.size != AuthSampleSize");
        return false;// not all signs gotted
    }

    for (auto& a : outv) {

		if( !CheckSign(a.FSN_StakeWalletAddr, a.Sign) ) return false;
		m_Signs.push_back(a.Sign);
        LOG_PRINT_L0("pushing sign " << a.Sign << " to tx,  checked with address: " << a.FSN_StakeWalletAddr);

	}



	if( !PutTXToPool() ) return false;

	rpc_command::WALLET_PUT_TX_IN_POOL::request req;
	req.PaymentID = TransactionRecord.PaymentID;
	req.TransactionPoolID = m_TransactionPoolID;
	//LOG_PRINT_L5("PaymentID: "<<TransactionRecord.PaymentID);

	vector<rpc_command::WALLET_PUT_TX_IN_POOL::response> vv_out;

	if( !m_SubNetBroadcast.Send( dapi_call::WalletPutTxInPool, req, vv_out) ) return false;


	ADD_RTA_OBJECT_HANDLER(GetPayStatus, rpc_command::WALLET_GET_TRANSACTION_STATUS, WalletPayObject);


	return true;
}

bool supernode::WalletPayObject::GetPayStatus(const rpc_command::WALLET_GET_TRANSACTION_STATUS::request& in, rpc_command::WALLET_GET_TRANSACTION_STATUS::response& out) {
	out.Status = int(m_Status);
	//TimeMark -= boost::posix_time::hours(3);
    out.Result = STATUS_OK;
    return true;
}




bool supernode::WalletPayObject::PutTXToPool() {
	// TODO: IMPL. all needed data we have in TransactionRecord + m_Signs.
	// TODO: Result, monero_tranaction_id must be putted to m_TransactionPoolID

    // TODO: send tx to blockchain
    // Things we need here here

    // 1. destination address:
    // TransactionRecord.POSAddress;
    // 2. amount
    // TransactionRecord.Amount;
    // 3. wallet -> we opened it previously with OpenSenderWallet
    if (!m_wallet) {
        LOG_ERROR("Wallet needs to be opened with OpenSenderWallet before this call");
        return false;
    }

    GraftTxExtra tx_extra;
    tx_extra.BlockNum = 123;
    tx_extra.PaymentID = TransactionRecord.PaymentID;
    tx_extra.Signs = m_Signs;

//    for (auto &sign : tx_extra.Signs) {
//        LOG_PRINT_L0("pushing sign to tx extra: " << sign);
//    }

    std::unique_ptr<PendingTransaction> ptx {
            m_wallet->createTransaction(TransactionRecord.POSAddress,
                                      "",
                                      TransactionRecord.Amount,
                                      0,
                                      tx_extra,
                                      Monero::PendingTransaction::Priority_Medium
                                      )};

    if (ptx->status() != PendingTransaction::Status_Ok) {
        LOG_ERROR("Failed to create tx: " << ptx->errorString());
        return false;
    }

    if (!ptx->commit()) {
        LOG_ERROR("Failed to send tx: " << ptx->errorString());
        return false;
    }

    if (ptx->txCount() == 0) {
        LOG_ERROR("Interlal error: txCount == 0");
        return false;
    }
    if (ptx->txCount() > 1) {
        LOG_ERROR("TODO: we should handle this somehow");
        throw std::runtime_error(std::string("tx was splitted by ") + std::to_string(ptx->txCount()) + " transactions, we dont hadle it now");
    }

    m_TransactionPoolID = ptx->txid()[0];
    return true;
}


