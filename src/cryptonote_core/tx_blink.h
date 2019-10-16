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

#pragma once

#include "../cryptonote_basic/cryptonote_basic.h"
#include "../common/util.h"
#include "service_node_rules.h"
#include <iostream>

namespace service_nodes {
class service_node_list;
}

namespace cryptonote {

// FIXME TODO XXX - rename this file to blink_tx.h

class blink_tx {
public:
  enum class subquorum : uint8_t { base, future, _count };

  class signature_verification_error : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  /**
   * Construct a new blink_tx wrapper given the tx and a blink authorization height.
   */
  blink_tx(std::shared_ptr<transaction> tx, uint64_t height)
    : tx_{std::move(tx)}, height_{height} {
    signatures_.fill({});
  }

  /** Construct a new blink_tx from just a height; constructs a default transaction.
   */
  explicit blink_tx(uint64_t height) : blink_tx(std::make_shared<transaction>(), height) {}

  /**
   * Adds a signature for the given quorum and position.  Returns true if the signature was accepted
   * (i.e. is valid, and existing signature is empty), false if the signature was already present,
   * and throws a `blink_tx::signature_verification_error` if the signature fails validation.
   */
  bool add_signature(subquorum q, unsigned int position, const crypto::signature &sig, const service_nodes::service_node_list &snl);

  /**
   * Remove the signature at the given quorum and position by setting it to null.  Returns true if
   * removed, false if it was already null.
   */
  bool clear_signature(subquorum q, unsigned int position);

  /**
   * Returns true if there is a verified signature at the given quorum and position.
   */
  bool has_signature(subquorum q, unsigned int position);

  /**
   * Returns true if this blink tx is valid for inclusion in the blockchain, that is, has the
   * required number of valid signatures in each quorum.
   */
  bool valid() const;

  /// Returns a reference to the transaction.
  transaction &tx() { return *tx_; }

  /// Returns a reference to the transaction, const version.
  const transaction &tx() const { return *tx_; }

  /// Returns the blink authorization height of this blink tx, i.e. the block height at the time the
  /// transaction was created.
  uint64_t height() const { return height_; }

  /// Returns the quorum height for the given height and quorum (base or future); returns 0 at the
  /// beginning of the chain (before there are enough blocks for a blink quorum).
  static uint64_t quorum_height(uint64_t h, subquorum q) {
    uint64_t bh = h - (h % service_nodes::BLINK_QUORUM_INTERVAL) - service_nodes::BLINK_QUORUM_LAG
      + static_cast<uint8_t>(q) * service_nodes::BLINK_QUORUM_INTERVAL;
    return bh > h /*overflow*/ ? 0 : bh;
  }

  /// Returns the quorum height for the given quorum (base or future); returns 0 at the beginning of
  /// the chain (before there are enough blocks for a blink quorum).
  uint64_t quorum_height(subquorum q) const { return quorum_height(height_, q); }

  /// Returns the pubkey of the referenced service node, or null if there is no such service node.
  crypto::public_key get_sn_pubkey(subquorum q, unsigned position, const service_nodes::service_node_list &snl) const;

  /// Returns the hashed signing value for this blink TX (a fast hash of the height + tx hash)
  crypto::hash hash() const;

private:
  std::shared_ptr<transaction> tx_;
  uint64_t height_;
  std::array<std::array<crypto::signature, service_nodes::BLINK_SUBQUORUM_SIZE>, tools::enum_count<subquorum>> signatures_;

};

}
