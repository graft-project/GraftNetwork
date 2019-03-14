#include "utils.h"
#include <cryptonote_basic/cryptonote_format_utils.h>
#include <ringct/rctSigs.h>


using namespace cryptonote;

namespace Utils {

bool decode_ringct(const rct::rctSig& rv, const crypto::public_key pub, const crypto::secret_key &sec, unsigned int i, rct::key & mask, uint64_t & amount)
{
    crypto::key_derivation derivation;
    bool r = crypto::generate_key_derivation(pub, sec, derivation);
    if (!r)
    {
        LOG_ERROR("Failed to generate key derivation to decode rct output " << i);
        return false;
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

// copy-pasted from simplewallet.cpp;
// TODO: check if it possible to unify it with lookup_account_outputs_ringct
bool get_tx_amount(const account_public_address &address, const secret_key &key, const transaction &tx, std::vector<std::pair<size_t, uint64_t> > &outputs, uint64_t &total_transfered)
{

  crypto::key_derivation derivation;
  if (!crypto::generate_key_derivation(address.m_view_public_key, key, derivation))
  {
    MERROR("failed to generate key derivation from supplied parameters");
    return true;
  }

  try {
    for (size_t n = 0; n < tx.vout.size(); ++n)
    {
      if (typeid(txout_to_key) != tx.vout[n].target.type())
        continue;
      const txout_to_key tx_out_to_key = boost::get<txout_to_key>(tx.vout[n].target);
      crypto::public_key pubkey;
      derive_public_key(derivation, n, address.m_spend_public_key, pubkey);
      if (pubkey == tx_out_to_key.key)
      {
        uint64_t amount;
        if (tx.version == 1)
        {
          amount = tx.vout[n].amount;
        }
        else
        {
          try
          {
            rct::key Ctmp;
            {
              crypto::secret_key scalar1;
              crypto::derivation_to_scalar(derivation, n, scalar1);
              rct::ecdhTuple ecdh_info = tx.rct_signatures.ecdhInfo[n];
              rct::ecdhDecode(ecdh_info, rct::sk2rct(scalar1));
              rct::key C = tx.rct_signatures.outPk[n].mask;
              rct::addKeys2(Ctmp, ecdh_info.mask, ecdh_info.amount, rct::H);
              if (rct::equalKeys(C, Ctmp))
                amount = rct::h2d(ecdh_info.amount);
              else
                amount = 0;
            }
          }
          catch (...) { amount = 0; }
        }
        total_transfered += amount;
      }
    }
  }
  catch(const std::exception &e)
  {
    LOG_ERROR("error: " << e.what());
    return false;
  }
  return true;
}

} // namespace Utils
