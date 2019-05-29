// Copyright (c) 2019, The Graft Project
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
// Parts of this file are originally copyright (c) 2014-2019 The Monero Project

#include <sstream>

#include "common/rta_kit.h"
#include "crypto/hash.h"
#include "crypto/crypto.h"
#include "misc_log_ex.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"

#include "utils/sample_generator.h" // bad idea to include the whole heavy header just 
// because of one const - graft::generator::AUTH_SAMPLE_SIZE
// It's better to hold all const's in separate lightweight header

namespace rta::flow2::validation {

bool belongs_to_auth_sample(const std::vector<crypto::public_key>& auth_sample_pkeys, 
  const rta_header& rta_hdr, const u32 auth_sample_pkeys_off)
{
  bool ok = false;  // ok is true when every supernode from rta_hdr is member of auth_sample
  for(u32 i = auth_sample_pkeys_off, cnt = rta_hdr.keys.size(); i < cnt; ++i)
  {
    const auto& key = rta_hdr.keys[i];
    if(!(ok = std::any_of(auth_sample_pkeys.cbegin(), auth_sample_pkeys.cend(),
      [&key](const crypto::public_key& k) { return key == k; })))
    {
      MERROR("Key " << key << " does not belong to auth-sample");
      break;
    }
  }

#if 0
  {
    std::ostringstream m;
    m << std::endl << "### DBG: keys to be checked against belonging to auth sample (cnt:"
      << (rta_hdr.keys.size() - auth_sample_pkeys_off) << "):";

    for(u32 i = auth_sample_pkeys_off, cnt = rta_hdr.keys.size(); i < cnt; ++i)
      m << std::endl << rta_hdr.keys[i];

    m << std::endl << "### DBG: keys of auth sample (cnt:" << auth_sample_pkeys.size() << "):";
    for(const auto& k : auth_sample_pkeys) m << std::endl << k;
    MDEBUG(m.str());
  }
#endif

  return ok;
}

bool check_rta_signatures(const std::vector<rta_signature>& rta_signs,
  const rta_header& rta_hdr, const crypto::hash& txid, const u32 auth_sample_pkeys_off)
{
  bool ok = true;
  for(const auto& rs : rta_signs)
  {
    const i32 idx = rs.key_index + auth_sample_pkeys_off;

    if(idx > (i32)(rta_hdr.keys.size() - 1))
    {
      MERROR("Fail at check_rta_signatures - out of index!");
      return false;
    }

    const auto& key = rta_hdr.keys[idx];
    if(!crypto::check_signature(txid, key, rs.signature))
    {
      MERROR("Failed to validate rta tx " << std::endl
        << "signature: " << epee::string_tools::pod_to_hex(rs.signature) << std::endl
        << "for key: " << key << std::endl
        << "tx-id:    " << epee::string_tools::pod_to_hex(txid));
      ok = false;
      break;
    }
  }
  return ok;
}

bool check_rta_keys_count(const rta_header& rta_hdr, const crypto::hash& txid)
{
  const uint32_t cnt = rta_hdr.keys.size();

  // so far there can be cases when we have only graft::generator::AUTH_SAMPLE_SIZE
  // records (3 starting are missing)
  const bool ok = (cnt == graft::generator::AUTH_SAMPLE_SIZE) 
    || (cnt == (3 + graft::generator::AUTH_SAMPLE_SIZE));

  if(!ok)
    MERROR("Failed to validate rta tx, wrong amount ("
      << cnt << ") of auth sample keys for tx:" << txid << ". Expected "
      << (3 + graft::generator::AUTH_SAMPLE_SIZE));

  return ok;
}

bool check_rta_sign_key_indexes(const std::vector<rta_signature>& rta_signs,
  const crypto::hash& txid, const u32 auth_sample_pkeys_off)
{
  bool ok = true;
  for(const auto& rs : rta_signs)
  {
    if((rs.key_index < auth_sample_pkeys_off)
      || (rs.key_index > (auth_sample_pkeys_off + graft::generator::AUTH_SAMPLE_SIZE - 1)))
    {
      MERROR("Signature: " << rs.signature << " has wrong key index: "
        << rs.key_index << ", tx: " << txid);
      ok = false;
      break;
    }
  }
  return ok;
}

bool check_rta_sign_count(const std::vector<rta_signature>& rta_signs, const crypto::hash& txid)
{
  const uint32_t cnt = rta_signs.size();
  // according to spec/design there can be 6-8 signatures and we do check it in here
  const bool ok = !((cnt < 6) || (cnt > graft::generator::AUTH_SAMPLE_SIZE));
  if(!ok)
    MERROR("Wrong amount of signatures:" << cnt << " for tx:" << txid << " It should be 6-"
      << graft::generator::AUTH_SAMPLE_SIZE << ".");
  return ok;
}

u32 get_auth_sample_public_key_offset(const rta_header& rta_hdr)
{
  return (rta_hdr.keys.size() == 8) ? 0 : 3; // supernode public keys offset
}

}


