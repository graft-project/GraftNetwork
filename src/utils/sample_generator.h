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

#include "crypto/crypto.h"
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

constexpr int32_t TIERS = 4;
constexpr int32_t ITEMS_PER_TIER = 2;
constexpr int32_t AUTH_SAMPLE_SIZE = TIERS * ITEMS_PER_TIER;
constexpr int32_t BBQS_SIZE = TIERS * ITEMS_PER_TIER;
constexpr int32_t QCL_SIZE = TIERS * ITEMS_PER_TIER;
constexpr int32_t REQUIRED_BBQS_VOTES = (BBQS_SIZE*2 + (3-1))/3;
constexpr int32_t REQUIRED_DISQUAL2_VOTES = 5;

/*!
 * \brief selectSample - selects a sample such as BBQS and QCl.
 *
 * \param sample_size - required resulting size.
 * \param bbl_tiers - tiers of somehow sorted items.
 * \param out - resulting flat list.
 * \param prefix - it is for logging.
 * \return  - false if resulting size less than requested sample_size
 */
template<typename T, typename Tiers = std::array<std::vector<T>, TIERS>>
bool selectSample(size_t sample_size, const Tiers& bbl_tiers, std::vector<T>& out, const char* prefix)
{
    assert(sample_size % TIERS == 0);

    //select sample_size for each tier
    std::array<std::vector<T>, TIERS> tier_supernodes;
    for (size_t i=0; i<TIERS; ++i)
    {
        auto& src = bbl_tiers[i];
        auto& dst = tier_supernodes[i];
        dst.reserve(sample_size);
        uniform_select(do_not_seed{}, sample_size, src, dst);
        MDEBUG("..." << dst.size() << " supernodes has been selected for tier " << (i + 1) << " from blockchain based list with " << src.size() << " supernodes");
    }

    auto items_per_tier = sample_size / TIERS;

    std::array<int, TIERS> select;
    select.fill(items_per_tier);
    // If we are short of the needed SNs on any tier try selecting additional SNs from the highest
    // tier with surplus SNs.  For example, if tier 2 is short by 1, look for a surplus first at
    // tier 4, then tier 3, then tier 1.
    for (int i = 0; i < TIERS; i++) {
        int deficit_i = select[i] - int(tier_supernodes[i].size());
        for (int j = TIERS-1; deficit_i > 0 && j >= 0; j--) {
            if (i == j) continue;
            int surplus_j = int(tier_supernodes[j].size()) - select[j];
            if (surplus_j > 0) {
                // Tier j has more SNs than needed, so select an extra SN from tier j to make up for
                // the deficiency at tier i.
                int transfer = std::min(deficit_i, surplus_j);
                select[i] -= transfer;
                select[j] += transfer;
                deficit_i -= transfer;
            }
        }
        // If we still have a deficit then no other tier has a surplus; we'll just have to work with
        // a smaller sample because there aren't enough SNs on the entire network.
        if (deficit_i > 0)
            select[i] -= deficit_i;
    }

    out.clear();
    out.reserve(sample_size);
    auto out_it = back_inserter(out);
    for (int i = 0; i < TIERS; i++) {
        std::copy(tier_supernodes[i].begin(), tier_supernodes[i].begin() + select[i], out_it);
    }

    if (out.size() > sample_size)
      out.resize(sample_size);

    MDEBUG("..." << out.size() << " supernodes has been selected");

    return out.size() == sample_size;
}

/*!
 * \brief select_BBQS_QCL - generates BBQS and QCl from bbl.
 *
 * \param block_hash - hash of the block corresponding to BBL.
 * \param bbl_tiers - tiers of somehow sorted items.
 * \param bbqs - resulting BBQS.
 * \param qcl - resulting QCL.
 * \return  - false if at least one of the resulting sizes less than desired sizes of BBQS or QCL
 */

template<typename T, typename Tiers = std::array<std::vector<T>, TIERS>>
bool select_BBQS_QCL(crypto::hash block_hash, const Tiers& bbl_tiers, std::vector<T>& bbqs, std::vector<T>& qcl)
{
    //seed once
    generator::seed_uniform_select(block_hash);
    bool res1 = selectSample(BBQS_SIZE, bbl_tiers, bbqs, "BBQS");
    bool res2 = selectSample(QCL_SIZE, bbl_tiers, qcl, "QCL");
    return res1 && res2;
}

/*!
 * \brief select_AuthSample - generates auth sample from bbl based on payment_id.
 *
 * \param payment_id
 * \param bbl_tiers - tiers of somehow sorted items.
 * \param auths - resulting auth sample.
 * \return  - false if resulting size less than desired auth sample size
 */

template<typename T, typename Tiers = std::array<std::vector<T>, TIERS>>
bool select_AuthSample(const std::string& payment_id, const Tiers& bbl_tiers, std::vector<T>& auths)
{
    //seed once
    generator::seed_uniform_select(payment_id);
    bool res = selectSample(AUTH_SAMPLE_SIZE, bbl_tiers, auths, "auth");
    return res;
}

}} //namespace graft::generator
