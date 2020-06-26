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
//



#ifndef RTA_TX_H
#define RTA_TX_H

#include "cryptonote_basic/cryptonote_basic.h"
#include "crypto/hash.h"

namespace cryptonote {

class StakeTransactionProcessor;

class rta_tx_error : public std::domain_error 
{
public:
  rta_tx_error(const std::string &msg);
  
};

class rta_tx
{
public:
  
  explicit rta_tx(const cryptonote::transaction &tx, StakeTransactionProcessor * stp);
  ~rta_tx() = default;
  const cryptonote::rta_header & rta_header() const { return m_rta_hdr; }
  
private:
  void parse(const cryptonote::transaction &tx);
  void validate(const cryptonote::transaction &tx, StakeTransactionProcessor * stp);
  
private:
  cryptonote::rta_header m_rta_hdr;
  std::vector<cryptonote::rta_signature> m_rta_signatures;
  crypto::hash m_txhash;
  
};

} // namespace cryptonote
#endif // RTA_TX_H
