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

#pragma once

#include <type_traits>
#include <random>
#include <algorithm>
#include <array>
#include "misc_log_ex.h"

namespace graft { namespace generator {

namespace detail
{

std::mt19937_64& get_rnd();

} //namespace detail

template<typename T>
void uniform_select(std::mt19937_64& rnd, size_t count, const std::vector<T>& src, std::vector<T>& dst)
{
    size_t src_size = src.size();
    if (count > src_size) count = src_size;
    for (size_t i=0; i<src_size; ++i)
    {
      size_t dice = rnd() % (src_size - i);
      if (dice >= count)
        continue;
      dst.push_back(src[i]);
      --count;
    }
}

struct do_not_seed{};

void seed_uniform_select(std::seed_seq& sseq);

template<typename POD>
typename std::enable_if< !std::is_same< typename std::decay<POD>::type, std::seed_seq>::value >::type
seed_uniform_select(const POD& pod)
{
    static_assert( !std::is_same<typename std::decay<POD>::type, do_not_seed>::value );
    static_assert( !std::is_same<typename std::decay<POD>::type, std::string>::value );
    static_assert(std::is_trivially_copyable<POD>::value);
    static_assert(sizeof(POD) % sizeof(uint32_t) == 0);

    auto* pb = reinterpret_cast<const uint32_t*>(&pod);
    auto* pe = pb + sizeof(POD) / sizeof(uint32_t);
    std::seed_seq sseq(pb, pe);
    seed_uniform_select(sseq);
}

void seed_uniform_select(const std::string& str);

template<typename T, typename DNS>
typename std::enable_if< std::is_same<typename std::decay<DNS>::type, do_not_seed>::value>::type
uniform_select(DNS&& tmp, size_t count, const std::vector<T>& src, std::vector<T>& dst)
{
    auto& rnd = detail::get_rnd();
    uniform_select(rnd, count, src, dst);
}

template<typename T>
void uniform_select(std::seed_seq& seed, size_t count, const std::vector<T>& src, std::vector<T>& dst)
{
    auto& rnd = detail::get_rnd();
    rnd.seed(seed);
    uniform_select(rnd, count, src, dst);
}

template<typename T, typename POD>
typename std::enable_if< !std::is_same<typename std::decay<POD>::type, do_not_seed>::value >::type
uniform_select(const POD& seed, size_t count, const std::vector<T>& src, std::vector<T>& dst)
{
    seed_uniform_select(seed);
    uniform_select(do_not_seed{}, count, src, dst);
}

}} //namespace graft::crypto_tools
