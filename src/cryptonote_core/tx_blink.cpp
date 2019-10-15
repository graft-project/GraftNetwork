#include "tx_blink.h"
#include "common/util.h"
#include <algorithm>

namespace cryptonote {

static void check_args(blink_tx::quorum q, unsigned position, const char *func_name) {
  if (q >= blink_tx::quorum::_count)
    throw std::domain_error("Invalid sub-quorum value passed to " + std::string(func_name));
  if (position >= BLINK_QUORUM_SIZE)
    throw std::domain_error("Invalid voter position passed to " + std::string(func_name));
}

crypto::public_key get_sn_pubkey(blink_tx::quorum q, unsigned position) {
  check_args(q, position, __func__);
  // TODO
  return crypto::null_pkey;
};

crypto::hash blink_tx::hash() const {
  auto buf = tools::memcpy_le(height_, tx_.hash);
  crypto::hash hash;
  crypto::cn_fast_hash(buf.data(), buf.size(), hash);
  return hash;
}

bool blink_tx::add_signature(quorum q, unsigned position, const crypto::signature &sig) {
  check_args(q, position, __func__);

  auto &sig_slot = signatures_[static_cast<uint8_t>(q)][position];
  if (sig_slot && sig_slot == sig)
    return false;

  if (!crypto::check_signature(hash(), get_sn_pubkey(q, position), sig))
    throw signature_verification_error("Given blink quorum signature verification failed!");

  sig_slot = sig;
}

bool blink_tx::has_signature(quorum q, unsigned position) {
  check_args(q, position, __func__);
  return signatures_[static_cast<uint8_t>(q)][position];
}

bool blink_tx::valid() const {
  // Signatures are verified when added, so here we can just test that they are non-null
  return std::all_of(signatures_.begin(), signatures_.end(), [](const auto &sigs) {
    return BLOCK_QUORUM_VOTES_REQUIRED <= std::count_if(sigs.begin(), sigs.end(),
      [](const auto &s) -> bool { return s; });
  });
}

}
