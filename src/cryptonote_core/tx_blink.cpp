#include "tx_blink.h"
#include "common/util.h"
#include "service_node_list.h"
#include <algorithm>

namespace cryptonote {

using namespace service_nodes;

static void check_args(blink_tx::subquorum q, unsigned position, const char *func_name) {
  if (q >= blink_tx::subquorum::_count)
    throw std::domain_error("Invalid sub-quorum value passed to " + std::string(func_name));
  if (position >= BLINK_SUBQUORUM_SIZE)
    throw std::domain_error("Invalid voter position passed to " + std::string(func_name));
}

crypto::public_key blink_tx::get_sn_pubkey(subquorum q, unsigned position, const service_node_list &snl) const {
  check_args(q, position, __func__);
  uint64_t qheight = quorum_height(q);
  auto blink_quorum = snl.get_quorum(quorum_type::blink, qheight);
  if (!blink_quorum) {
    // TODO FIXME XXX - we don't want a failure here; if this happens we need to go back into state
    // history to retrieve the state info.
    MERROR("FIXME: could not get blink quorum for blink_tx");
    return crypto::null_pkey;
  }

  if (position < blink_quorum->validators.size())
    return blink_quorum->validators[position];

  return crypto::null_pkey;
};

crypto::hash blink_tx::hash() const {
  auto buf = tools::memcpy_le(height_, tx_->hash);
  crypto::hash hash;
  crypto::cn_fast_hash(buf.data(), buf.size(), hash);
  return hash;
}

bool blink_tx::add_signature(subquorum q, unsigned position, const crypto::signature &sig, const service_node_list &snl) {
  check_args(q, position, __func__);

  auto &sig_slot = signatures_[static_cast<uint8_t>(q)][position];
  if (sig_slot && sig_slot == sig)
    return false;

  if (!crypto::check_signature(hash(), get_sn_pubkey(q, position, snl), sig))
    throw signature_verification_error("Given blink quorum signature verification failed!");

  sig_slot = sig;
  return true;
}

bool blink_tx::has_signature(subquorum q, unsigned position) {
  check_args(q, position, __func__);
  return signatures_[static_cast<uint8_t>(q)][position];
}

bool blink_tx::valid() const {
  // Signatures are verified when added, so here we can just test that they are non-null
  return std::all_of(signatures_.begin(), signatures_.end(), [](const auto &sigs) {
    return std::count_if(sigs.begin(), sigs.end(), [](const auto &s) -> bool { return s; }) >= int{BLINK_MIN_VOTES};
  });
}

}
