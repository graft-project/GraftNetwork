// Copyright (c) 2019, The Loki Project
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

#include "tx_blink.h"
#include "common/util.h"
#include "service_node_list.h"
#include <algorithm>
#include "../cryptonote_basic/cryptonote_format_utils.h"

namespace cryptonote {

using namespace service_nodes;

static void check_args(blink_tx::subquorum q, int position, const char *func_name) {
    if (q < blink_tx::subquorum::base || q >= blink_tx::subquorum::_count)
        throw std::invalid_argument("Invalid sub-quorum value passed to " + std::string(func_name));
    if (position < 0 || position >= BLINK_SUBQUORUM_SIZE)
        throw std::invalid_argument("Invalid voter position passed to " + std::string(func_name));
}

crypto::public_key blink_tx::get_sn_pubkey(subquorum q, int position, const service_node_list &snl) const {
    check_args(q, position, __func__);
    uint64_t qheight = quorum_height(q);
    auto blink_quorum = snl.get_quorum(quorum_type::blink, qheight);
    if (!blink_quorum) {
        // TODO FIXME XXX - we don't want a failure here; if this happens we need to go back into state
        // history to retrieve the state info.  (Or maybe this can't happen?)
        MERROR("FIXME: could not get blink quorum for blink_tx");
        return crypto::null_pkey;
    }

    if (position < (int) blink_quorum->validators.size())
        return blink_quorum->validators[position];

    return crypto::null_pkey;
};

crypto::hash blink_tx::hash(bool approved) const {
    auto buf = tools::memcpy_le(height, get_txhash().data, uint8_t{approved});
    crypto::hash blink_hash;
    crypto::cn_fast_hash(buf.data(), buf.size(), blink_hash);
    return blink_hash;
}


void blink_tx::limit_signatures(subquorum q, size_t max_size) {
    if (max_size > BLINK_SUBQUORUM_SIZE)
        throw std::domain_error("Internal error: too many potential blink signers!");
    else if (max_size < BLINK_SUBQUORUM_SIZE)
        for (size_t i = max_size; i < BLINK_SUBQUORUM_SIZE; i++)
            signatures_[static_cast<uint8_t>(q)][i].status = signature_status::rejected;
}

bool blink_tx::add_signature(subquorum q, int position, bool approved, const crypto::signature &sig, const crypto::public_key &pubkey) {
    check_args(q, position, __func__);

    if (!crypto::check_signature(hash(approved), pubkey, sig))
        throw signature_verification_error("Given blink quorum signature verification failed!");

    return add_prechecked_signature(q, position, approved, sig);
}


bool blink_tx::add_signature(subquorum q, int position, bool approved, const crypto::signature &sig, const service_node_list &snl) {
    return add_signature(q, position, approved, sig, get_sn_pubkey(q, position, snl));
}

bool blink_tx::add_prechecked_signature(subquorum q, int position, bool approved, const crypto::signature &sig) {
    check_args(q, position, __func__);

    auto &sig_slot = signatures_[static_cast<uint8_t>(q)][position];
    if (sig_slot.status != signature_status::none)
        return false;

    sig_slot.status = approved ? signature_status::approved : signature_status::rejected;
    sig_slot.sig = sig;
    return true;
}

blink_tx::signature_status blink_tx::get_signature_status(subquorum q, int position) const {
    check_args(q, position, __func__);
    return signatures_[static_cast<uint8_t>(q)][position].status;
}

bool blink_tx::approved() const {
    return std::all_of(signatures_.begin(), signatures_.end(), [](const auto &sigs) {
        return std::count_if(sigs.begin(), sigs.end(), [](const quorum_signature &s) -> bool { return s.status == signature_status::approved; })
                >= BLINK_MIN_VOTES;
    });
}

bool blink_tx::rejected() const {
    return std::any_of(signatures_.begin(), signatures_.end(), [](const auto &sigs) {
        return std::count_if(sigs.begin(), sigs.end(), [](const quorum_signature &s) -> bool { return s.status == signature_status::rejected; })
                > BLINK_SUBQUORUM_SIZE - BLINK_MIN_VOTES;
    });
}

void blink_tx::fill_serialization_data(crypto::hash &tx_hash, uint64_t &height, std::vector<uint8_t> &quorum, std::vector<uint8_t> &position, std::vector<crypto::signature> &signature) const {
    tx_hash = get_txhash();
    height = this->height;
    constexpr size_t res_size = tools::enum_count<subquorum> * service_nodes::BLINK_SUBQUORUM_SIZE;
    quorum.reserve(res_size);
    position.reserve(res_size);
    signature.reserve(res_size);
    for (uint8_t qi = 0; qi < signatures_.size(); qi++) {
        for (uint8_t p = 0; p < signatures_[qi].size(); p++) {
            auto &sig = signatures_[qi][p];
            if (sig.status == signature_status::approved) {
                quorum.push_back(qi);
                position.push_back(p);
                signature.push_back(sig.sig);
            }
        }
    }
}

crypto::hash blink_tx::tx_hash_visitor::operator()(const transaction &tx) const {
    crypto::hash h;
    if (!cryptonote::get_transaction_hash(tx, h))
        throw std::runtime_error("Failed to calculate transaction hash");
    return h;
}

}
