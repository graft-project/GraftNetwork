#include "PosSaleObject.h"
#include "TxPool.h"
#include "graft_defines.h"
#include <uuid/uuid.h>

using namespace cryptonote;

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

    // get tranaction from pool by in.TransactionPoolID
    // TODO: make txPool as a member of PosProxy or FSN_Servant?
    TxPool txPool(m_Servant->GetNodeAddress(), m_Servant->GetNodeLogin(), m_Servant->GetNodePassword());
    cryptonote::transaction tx;
    if (!txPool.get(in.TransactionPoolID, tx)) {
        LOG_ERROR("TX " << in.TransactionPoolID << " was not found in pool");
        return false;
    }
    // Get the tx extra
    GraftTxExtra graft_tx_extra;
    if (!cryptonote::get_graft_tx_extra_from_extra(tx, graft_tx_extra)) {
        LOG_ERROR("TX " << in.TransactionPoolID << " : error reading graft extra");
        return false;
    }
    // check all signs
    LOG_PRINT_L2("AuthNodes.size : " << TransactionRecord.AuthNodes.size());

    if (TransactionRecord.AuthNodes.size() != graft_tx_extra.Signs.size()) {
        LOG_ERROR("TX " << in.TransactionPoolID << " : number of auth nodes and number of signs mismatch");
        return false;
    }

    for (unsigned i = 0; i < graft_tx_extra.Signs.size(); ++i) {

        const string &sign = graft_tx_extra.Signs.at(i);
        bool check_result = false;
        // TODO: in some reasons TransactionRecord.AuthNodes order and Signs order mismatch, so it can't be checked
        // by the same index
        for (const auto & authNode : TransactionRecord.AuthNodes) {
            LOG_PRINT_L2("Checking signature with wallet: " << authNode->Stake.Addr << " [" << sign << "]");
            check_result = CheckSign(authNode->Stake.Addr, sign);
            if (check_result)
                break;
        }
        if (!check_result) {
            LOG_ERROR("TX " << in.TransactionPoolID << " : signature failed to check for all nodes: " << sign);
            return false;
        }
    }

    m_Status = NTransactionStatus::Success;
    return true;
}


bool supernode::PosSaleObject::PosRejectSale(const supernode::rpc_command::POS_REJECT_SALE::request &in, supernode::rpc_command::POS_REJECT_SALE::response &out) {
    m_Status = NTransactionStatus::RejectedByPOS;

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

