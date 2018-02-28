#include "utils.h"
#include <cryptonote_basic/cryptonote_format_utils.h>

using namespace cryptonote;

namespace Utils {

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


bool lookup_account_outputs_ringct(const account_keys &acc, const transaction &tx, std::vector<std::pair<size_t, uint64_t> > &outputs, uint64_t &total_transfered)
{
    crypto::public_key tx_pub_key = get_tx_pub_key_from_extra(tx);
    if (null_pkey == tx_pub_key)
        return false;

    total_transfered = 0;
    size_t output_idx = 0;

    LOG_PRINT_L1("tx pubkey: " << epee::string_tools::pod_to_hex(tx_pub_key));
    for(const tx_out& o:  tx.vout) {
        CHECK_AND_ASSERT_MES(o.target.type() ==  typeid(txout_to_key), false, "wrong type id in transaction out" );
        if (is_out_to_acc(acc, boost::get<txout_to_key>(o.target), tx_pub_key, output_idx)) {
            uint64_t rct_amount = 0;
            rct::key mask = tx.rct_signatures.ecdhInfo[output_idx].mask;
            if (decode_ringct(tx.rct_signatures, tx_pub_key, acc.m_view_secret_key, output_idx,
                              mask, rct_amount)) {
               total_transfered += rct_amount;
               outputs.push_back(make_pair(output_idx, rct_amount));
            }

        }
        output_idx++;
    }
    return true;
}

} // namespace Utils
