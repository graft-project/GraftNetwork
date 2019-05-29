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

#include <cstdint> // for std::uint32_t and such
#include <vector>

namespace cryptonote { struct rta_signature; struct rta_header; }
namespace crypto { struct hash; struct public_key; }

namespace rta::flow2::validation {

using i32 = std::int32_t;
using u32 = std::uint32_t;

using cryptonote::rta_signature;
using cryptonote::rta_header;

bool belongs_to_auth_sample(const std::vector<crypto::public_key>& auth_sample_pkeys, 
  const rta_header& rta_hdr, u32 auth_sample_pkeys_off);

bool check_rta_signatures(const std::vector<rta_signature>& rta_signs, const rta_header& rta_hdr, const crypto::hash& txid, u32 auth_sample_pkeys_off);

bool check_rta_sign_count(const std::vector<rta_signature>& rta_signs, const crypto::hash& txid);
bool check_rta_keys_count(const rta_header& rta_hdr, const crypto::hash& txid);
bool check_rta_sign_key_indexes(const std::vector<rta_signature>& rta_signs, const crypto::hash& txid, u32 auth_sample_pkeys_off);
u32 get_auth_sample_public_key_offset(const rta_header& rta_hdr);

}

