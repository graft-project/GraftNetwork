// Copyright (c) 2017, The Graft Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "PosSaleObject.h"
#include "TxPool.h"
#include "graft_defines.h"
#include <utils/utils.h>
#include <ringct/rctSigs.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>


using namespace cryptonote;


namespace  {

// TODO: move these functions to some "Utils" class/library



} // namespace

void supernode::PosSaleObject::Owner(PosProxy* o) { m_Owner = o; }

bool supernode::PosSaleObject::Init(const RTA_TransactionRecordBase& src) {
	BaseRTAObject::Init(src);

	TransactionRecord.PaymentID = GeneratePaymentID();
	TransactionRecord.BlockNum = m_Servant->GetCurrentBlockHeight();
	TransactionRecord.AuthNodes = m_Servant->GetAuthSample( TransactionRecord.BlockNum );
    if( TransactionRecord.AuthNodes.empty() ) { LOG_PRINT_L0("SALE: AuthNodes.empty"); m_Status = NTransactionStatus::Fail; return false; }

    m_Status = NTransactionStatus::InProgress;

	ADD_RTA_OBJECT_HANDLER(GetSaleStatus, rpc_command::POS_GET_SALE_STATUS, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(PoSTRSigned, rpc_command::POS_TR_SIGNED, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(PosRejectSale, rpc_command::POS_REJECT_SALE, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(AuthWalletRejectPay, rpc_command::WALLET_REJECT_PAY, PosSaleObject);

	return true;
}

void supernode::PosSaleObject::ContinueInit() {
	InitSubnet();

	vector<rpc_command::POS_PROXY_SALE::response> outv;
	rpc_command::POS_PROXY_SALE::request inbr;
	rpc_command::ConvertFromTR(inbr, TransactionRecord);
	inbr.SenderIP = m_DAPIServer->IP();
	inbr.SenderPort = m_DAPIServer->Port();
	if( !m_SubNetBroadcast.Send(dapi_call::PosProxySale, inbr, outv) || outv.empty() ) {
        LOG_ERROR("!Send dapi_call::PosProxySale");
		m_Status = NTransactionStatus::Fail;

	}

}

bool supernode::PosSaleObject::AuthWalletRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out) {
	LOG_PRINT_L0("PosSaleObject::AuthWalletRejectPay" << in.PaymentID);
	m_Status = NTransactionStatus::RejectedByWallet;
	return true;
}

bool supernode::PosSaleObject::GetSaleStatus(const rpc_command::POS_GET_SALE_STATUS::request& in, rpc_command::POS_GET_SALE_STATUS::response& out)
{
	LOG_PRINT_L0("PosSaleObject::GetSaleStatus" << in.PaymentID);
	out.Status = int(m_Status);
    out.Result = STATUS_OK;
	return true;
}


bool supernode::PosSaleObject::PoSTRSigned(const rpc_command::POS_TR_SIGNED::request& in, rpc_command::POS_TR_SIGNED::response& out) {
	LOG_PRINT_L0("PosSaleObject::PoSTRSigned" << in.PaymentID);
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
    //LOG_PRINT_L2("AuthNodes.size : " << TransactionRecord.AuthNodes.size());

    if (TransactionRecord.AuthNodes.size() != graft_tx_extra.Signs.size()) {
        LOG_ERROR("TX " << in.TransactionPoolID << " : number of auth nodes and number of signs mismatch");
        return false;
    }

    for (unsigned i = 0; i < graft_tx_extra.Signs.size(); ++i) {
        const string &sign = graft_tx_extra.Signs.at(i);

        if( CheckSign(TransactionRecord.AuthNodes[i]->Stake.Addr, sign) ) {
        	m_Signs++;
        } else {
        	LOG_ERROR("TX " << in.TransactionPoolID << " : signature failed to check for all nodes: " << sign);
        }

        /*
        bool check_result = false;
        // TODO: in some reasons TransactionRecord.AuthNodes order and Signs order mismatch, so it can't be checked
        // by the same index
        for (const auto & authNode : TransactionRecord.AuthNodes) {
            check_result = CheckSign(authNode->Stake.Addr, sign);
            if (check_result)
                break;
        }
        if (!check_result) {
            LOG_ERROR("TX " << in.TransactionPoolID << " : signature failed to check for all nodes: " << sign);
            return false;
        }

        m_Signs++;
           */
    }

    if (m_Signs != m_Servant->AuthSampleSize()) {
        LOG_ERROR("Checked signs number mismatch with auth sample size, " << m_Signs << "/" << m_Servant->AuthSampleSize());
        return false;
    }


    //  check transaction amount against TransactionRecord.Amount using TransactionRecord.POSViewKey
    // get POS address and view key
    cryptonote::account_keys pos_account;
    epee::string_tools::hex_to_pod(TransactionRecord.POSViewKey, pos_account.m_view_secret_key);

    if (!cryptonote::get_account_address_from_str(pos_account.m_account_address, m_Servant->IsTestnet(), TransactionRecord.POSAddress)) {
        LOG_ERROR("Error parsing POS Address: " << TransactionRecord.POSAddress);
        return false;
    }


    uint64_t amount = 0;
    vector<pair<size_t, uint64_t>> outputs;

    if (!Utils::lookup_account_outputs_ringct(pos_account, tx, outputs, amount)) {
        LOG_ERROR("Error checking tx outputs");
        return false;
    }

    if (amount != TransactionRecord.Amount) {
        LOG_ERROR("Tx amount is not equal to TransactionRecord.Amount: " << amount << "/" << TransactionRecord.Amount);
        return false;
    }

    m_Status = NTransactionStatus::Success;
    return true;
}


bool supernode::PosSaleObject::PosRejectSale(const supernode::rpc_command::POS_REJECT_SALE::request &in, supernode::rpc_command::POS_REJECT_SALE::response &out) {
	LOG_PRINT_L0("PosSaleObject::PosRejectSale" << in.PaymentID);
    m_Status = NTransactionStatus::RejectedByPOS;

    //TODO: Add impl

    out.Result = STATUS_OK;
    return true;
}


string supernode::PosSaleObject::GeneratePaymentID() {
    boost::uuids::random_generator gen;
    boost::uuids::uuid id = gen();
    return boost::uuids::to_string(id);
}

