// Copyright (c)      2020, The Graft Project
// Copyright (c)      2018, The Loki Project
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

#include "serialization/serialization.h"
#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include <boost/serialization/access.hpp>

namespace rta
{
  
struct checkpoint_vote 
{ 
  uint8_t version = 0;
  crypto::hash block_hash;
  uint64_t block_height;
  cryptonote::rta_signature signature;
  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(version)
    KV_SERIALIZE(block_height)
    KV_SERIALIZE_VAL_POD_AS_BLOB(block_hash)
    KV_SERIALIZE_VAL_POD_AS_BLOB(signature)
    // KV_SERIALIZE_VAL_POD_AS_BLOB_N(checkpoint.block_hash, "checkpoint")
  END_KV_SERIALIZE_MAP()
  private:
   friend class boost::serialization::access;
   template <class Archive>
   void serialize(Archive &ar, const unsigned int /*version*/) { }
};

} // namespace rta
