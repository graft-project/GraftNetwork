// Copyright (c) 2020, The Graft Project
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


#include "rta_tx.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include <sstream>


namespace cryptonote {
using namespace std; 

rta_tx_error::rta_tx_error(const std::string &msg)
  : std::domain_error(msg) 
{
}

rta_tx::rta_tx(const cryptonote::transaction &tx, cryptonote::StakeTransactionProcessor *stp)
{
  parse(tx);
  validate(tx, stp);
}

void rta_tx::parse(const transaction &tx)
{
  ostringstream oss;
  m_txhash = cryptonote::get_transaction_hash(tx);
  if (tx.type != transaction::tx_type_rta) {
    oss << "Transaction is not RTA: " << m_txhash;
    throw rta_tx_error(oss.str());
  }

  if (!cryptonote::get_graft_rta_header_from_extra(tx, m_rta_hdr)) {
    oss << "Failed to parse rta-header from tx extra: " << m_txhash;
    throw rta_tx_error(oss.str());
  }
  
  if (!cryptonote::get_graft_rta_signatures_from_extra2(tx, m_rta_signatures)) {
    oss << "Failed to parse rta signatures from tx extra: " << m_txhash;
    throw rta_tx_error(oss.str());
  }
}

void rta_tx::validate(const transaction &/*tx*/, StakeTransactionProcessor */*stp*/)
{
  static const size_t MIN_SIGNATURES = 6 + 3; // TODO: move to global config?
  ostringstream oss;
  if (m_rta_hdr.keys.size() == 0) {
    oss << "Failed to validate rta tx, missing auth sample keys for tx: " << m_txhash;
    throw rta_tx_error(oss.str());
  }

  if (m_rta_signatures.size() < MIN_SIGNATURES) {
    oss << "expected " << MIN_SIGNATURES << " rta signatures but only " << m_rta_signatures.size() << "given ";
    throw rta_tx_error(oss.str());
  }

  for (const auto &rta_sign : m_rta_signatures) {
    // check if key index is in range
    bool result = true;
    if (rta_sign.key_index >= m_rta_hdr.keys.size()) {
      oss << "signature: " << rta_sign.signature << " has wrong key index: " << rta_sign.key_index;
      throw(oss.str());
    }

    result &= crypto::check_signature(m_txhash, m_rta_hdr.keys[rta_sign.key_index], rta_sign.signature);
    if (!result) {
      oss << "Failed to validate rta tx signature: " << rta_sign.signature << ", tx: " << m_txhash << "key: " << m_rta_hdr.keys[rta_sign.key_index];
      throw(oss.str());
    }
    
    // TODO: check if tx signed by valid supernode
    
  }
}


}
