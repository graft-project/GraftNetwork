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
#include <iostream>

namespace cryptonote {

constexpr unsigned int BLINK_QUORUM_SIZE = 10;
constexpr unsigned int BLOCK_QUORUM_VOTES_REQUIRED = 7;

class blink_tx {
public:
  enum class quorum : uint8_t { base, future, _count };

  class signature_verification_error : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  /**
   * Construct a new blink_tx wrapper given the tx and a blink authorization height.
   */
  blink_tx(transaction tx, uint64_t height)
    : tx_{std::move(tx)}, height_{height} {}

  /**
   * Adds a signature for the given quorum and position.  Returns true if the signature was accepted
   * (i.e. is valid, and existing signature is empty), false if the signature was already present,
   * and throws a `blink_tx::signature_verification_error` if the signature fails validation.
   */
  bool add_signature(quorum q, unsigned int position, const crypto::signature &sig);

  /**
   * Remove the signature at the given quorum and position by setting it to null.  Returns true if
   * removed, false if it was already null.
   */
  bool clear_signature(quorum q, unsigned int position);

  /**
   * Returns true if there is a verified signature at the given quorum and position.
   */
  bool has_signature(quorum q, unsigned int position);

  /**
   * Returns true if this blink tx is valid for inclusion in the blockchain, that is, has the
   * required number of valid signatures in each quorum.
   */
  bool valid() const;

  /// Returns a reference to the transaction.
  const transaction &tx() const { return tx_; }

  /// Returns the blink authorization height of this blink tx
  uint64_t height() const { return height_; }

  /// Returns the hashed signing value for this blink TX (a fast hash of the height + tx hash)
  crypto::hash hash() const;

private:
  transaction tx_;
  uint64_t height_;
  std::array<std::array<crypto::signature, BLINK_QUORUM_SIZE>, tools::enum_count<quorum>> signatures_;

};

}
