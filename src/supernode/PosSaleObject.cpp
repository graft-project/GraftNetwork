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
#include <ringct/rctSigs.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>


using namespace cryptonote;


namespace  {

// TODO: move these functions to some "Utils" class/library

bool decode_ringct(const rct::rctSig& rv, const crypto::public_key pub, const crypto::secret_key &sec, unsigned int i, rct::key & mask, uint64_t & amount)
{
    crypto::key_derivation derivation;
    bool r = crypto::generate_key_derivation(pub, sec, derivation);
    if (!r)
    {
        LOG_ERROR("Failed to generate key derivation to decode rct output " << i);
        return 0;
    }
    crypto::secret_key scalar1;
    crypto::derivation_to_scalar(derivation, i, scalar1);
    try
    {
        switch (rv.type)
        {
        case rct::RCTTypeSimple:
            amount = rct::decodeRctSimple(rv, rct::sk2rct(scalar1), i, mask);
            break;
        case rct::RCTTypeFull:
            amount = rct::decodeRct(rv, rct::sk2rct(scalar1), i, mask);
            break;
        default:
            LOG_ERROR("Unsupported rct type: " << (int) rv.type);
            return false;
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to decode input " << i);
        return false;
    }
    return true;
}


bool lookup_acc_outs_rct(const account_keys &acc, const transaction &tx, std::vector<size_t> &outs, uint64_t &money_transfered)
{
    crypto::public_key tx_pub_key = get_tx_pub_key_from_extra(tx);
    if (null_pkey == tx_pub_key)
        return false;

    money_transfered = 0;
    size_t output_idx = 0;

    LOG_PRINT_L1("tx pubkey: " << epee::string_tools::pod_to_hex(tx_pub_key));
    for(const tx_out& o:  tx.vout) {
        CHECK_AND_ASSERT_MES(o.target.type() ==  typeid(txout_to_key), false, "wrong type id in transaction out" );
        if (is_out_to_acc(acc, boost::get<txout_to_key>(o.target), tx_pub_key, output_idx)) {
            uint64_t rct_amount = 0;
            rct::key mask = tx.rct_signatures.ecdhInfo[output_idx].mask;
            if (decode_ringct(tx.rct_signatures, tx_pub_key, acc.m_view_secret_key, output_idx,
                              mask, rct_amount))
                money_transfered += rct_amount;
        }
        output_idx++;
    }
    return true;
}

} // namespace

void supernode::PosSaleObject::Owner(PosProxy* o) { m_Owner = o; }

supernode::DAPICallResult supernode::PosSaleObject::Init(const RTA_TransactionRecordBase& src) {
	BaseRTAObject::Init(src);

	TransactionRecord.PaymentID = GeneratePaymentID();
	TransactionRecord.BlockNum = m_Servant->GetCurrentBlockHeight();
	TransactionRecord.AuthNodes = m_Servant->GetAuthSample( TransactionRecord.BlockNum );
	if( TransactionRecord.AuthNodes.empty() ) { m_Status = NTransactionStatus::Fail; return "SALE: AuthNodes.empty"; }

    m_Status = NTransactionStatus::InProgress;

	ADD_RTA_OBJECT_HANDLER(GetSaleStatus, rpc_command::POS_GET_SALE_STATUS, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(PoSTRSigned, rpc_command::POS_TR_SIGNED, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(PosRejectSale, rpc_command::POS_REJECT_SALE, PosSaleObject);
	ADD_RTA_OBJECT_HANDLER(AuthWalletRejectPay, rpc_command::WALLET_REJECT_PAY, PosSaleObject);

	return "";
}

void supernode::PosSaleObject::ContinueInit() {
	InitSubnet();

	vector<rpc_command::POS_PROXY_SALE::response> outv;
	rpc_command::POS_PROXY_SALE::request inbr;
	rpc_command::ConvertFromTR(inbr, TransactionRecord);
	inbr.SenderIP = m_DAPIServer->IP();
	inbr.SenderPort = m_DAPIServer->Port();
	if( !m_SubNetBroadcast.Send(dapi_call::PosProxySale, inbr, outv) || outv.empty() ) {
		LOG_PRINT_L5("!Send dapi_call::PosProxySale");
		m_Status = NTransactionStatus::Fail;

	}

}

supernode::DAPICallResult supernode::PosSaleObject::AuthWalletRejectPay(const rpc_command::WALLET_REJECT_PAY::request &in, rpc_command::WALLET_REJECT_PAY::response &out) {
	m_Status = NTransactionStatus::RejectedByWallet;
	return "";
}

supernode::DAPICallResult supernode::PosSaleObject::GetSaleStatus(const rpc_command::POS_GET_SALE_STATUS::request& in, rpc_command::POS_GET_SALE_STATUS::response& out)
{
	out.Status = int(m_Status);
    out.Result = STATUS_OK;
	return "";
}


supernode::DAPICallResult supernode::PosSaleObject::PoSTRSigned(const rpc_command::POS_TR_SIGNED::request& in, rpc_command::POS_TR_SIGNED::response& out) {
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_TxInPoolGotGuard);
		if(m_TxInPoolGot) return "";
		m_TxInPoolGot = true;
	}

    // get tranaction from pool by in.TransactionPoolID
    // TODO: make txPool as a member of PosProxy or FSN_Servant?
    TxPool txPool(m_Servant->GetNodeAddress(), m_Servant->GetNodeLogin(), m_Servant->GetNodePassword());
    cryptonote::transaction tx;
    if (!txPool.get(in.TransactionPoolID, tx)) {
        return string("TX ")+ boost::lexical_cast<string>(in.TransactionPoolID) + string(" was not found in pool");
    }


    // Get the tx extra
    GraftTxExtra graft_tx_extra;
    if (!cryptonote::get_graft_tx_extra_from_extra(tx, graft_tx_extra)) {
    	return string("TX ")+ boost::lexical_cast<string>(in.TransactionPoolID) + string(" : error reading graft extra");
    }
    // check all signs
    //LOG_PRINT_L2("AuthNodes.size : " << TransactionRecord.AuthNodes.size());

    if (TransactionRecord.AuthNodes.size() != graft_tx_extra.Signs.size()) {
    	return string("TX ")+ boost::lexical_cast<string>(in.TransactionPoolID) + string(" : number of auth nodes and number of signs mismatch");
    }


    //LOG_PRINT_L5("graft_tx_extra.Signs: "<<graft_tx_extra.Signs.size()<<"  TransactionRecord.AuthNodes: "<<TransactionRecord.AuthNodes.size());


    for (unsigned i = 0; i < graft_tx_extra.Signs.size(); ++i) {
        const string &sign = graft_tx_extra.Signs.at(i);

        if( CheckSign(TransactionRecord.AuthNodes[i]->Stake.Addr, sign) ) {
        	m_Signs++;
        } else {
        	return string("TX ")+ boost::lexical_cast<string>(in.TransactionPoolID) + string(" : signature failed to check for all nodes: ")+sign;
        }

    }

    if (m_Signs != m_Servant->AuthSampleSize()) {
        return string("Checked signs number mismatch with auth sample size, ") + boost::lexical_cast<string>(m_Signs) + string("/") + boost::lexical_cast<string>(m_Servant->AuthSampleSize());
    }


    //  check transaction amount against TransactionRecord.Amount using TransactionRecord.POSViewKey
    // get POS address and view key
    cryptonote::account_keys pos_account;
    epee::string_tools::hex_to_pod(TransactionRecord.POSViewKey, pos_account.m_view_secret_key);

    if (!cryptonote::get_account_address_from_str(pos_account.m_account_address, m_Servant->IsTestnet(), TransactionRecord.POSAddress)) {
        return string("Error parsing POS Address: ") + TransactionRecord.POSAddress;
    }


    uint64_t amount = 0;
    vector<size_t> outputs;

    if (!lookup_acc_outs_rct(pos_account, tx, outputs, amount)) {
        return("Error checking tx outputs");
    }

    if (amount != TransactionRecord.Amount) {
        return string("Tx amount is not equal to TransactionRecord.Amount: ") + boost::lexical_cast<string>(amount) + string("/") + boost::lexical_cast<string>(TransactionRecord.Amount);
    }

    m_Status = NTransactionStatus::Success;
    return "";
}


supernode::DAPICallResult supernode::PosSaleObject::PosRejectSale(const supernode::rpc_command::POS_REJECT_SALE::request &in, supernode::rpc_command::POS_REJECT_SALE::response &out) {
    m_Status = NTransactionStatus::RejectedByPOS;

    //TODO: Add impl

    out.Result = STATUS_OK;
    return "";
}


string supernode::PosSaleObject::GeneratePaymentID() {
    boost::uuids::random_generator gen;
    boost::uuids::uuid id = gen();
    return boost::uuids::to_string(id);
}

