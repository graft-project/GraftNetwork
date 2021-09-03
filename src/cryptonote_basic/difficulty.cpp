// Copyright (c) 2014-2019, The Monero Project
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
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "int-util.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "difficulty.h"
#include "include_base_utils.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "difficulty"

namespace cryptonote {

  using std::size_t;
  using std::uint64_t;
  using std::vector;

#if defined(__x86_64__)
  static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high) {
    low = mul128(a, b, &high);
  }

#else

  static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high) {
    // __int128 isn't part of the standard, so the previous function wasn't portable. mul128() in Windows is fine,
    // but this portable function should be used elsewhere. Credit for this function goes to latexi95.

    uint64_t aLow = a & 0xFFFFFFFF;
    uint64_t aHigh = a >> 32;
    uint64_t bLow = b & 0xFFFFFFFF;
    uint64_t bHigh = b >> 32;

    uint64_t res = aLow * bLow;
    uint64_t lowRes1 = res & 0xFFFFFFFF;
    uint64_t carry = res >> 32;

    res = aHigh * bLow + carry;
    uint64_t highResHigh1 = res >> 32;
    uint64_t highResLow1 = res & 0xFFFFFFFF;

    res = aLow * bHigh;
    uint64_t lowRes2 = res & 0xFFFFFFFF;
    carry = res >> 32;

    res = aHigh * bHigh + carry;
    uint64_t highResHigh2 = res >> 32;
    uint64_t highResLow2 = res & 0xFFFFFFFF;

    //Addition

    uint64_t r = highResLow1 + lowRes2;
    carry = r >> 32;
    low = (r << 32) | lowRes1;
    r = highResHigh1 + highResLow2 + carry;
    uint64_t d3 = r & 0xFFFFFFFF;
    carry = r >> 32;
    r = highResHigh2 + carry;
    high = d3 | (r << 32);
  }

#endif

  static inline bool cadd(uint64_t a, uint64_t b) {
    return a + b < a;
  }

  static inline bool cadc(uint64_t a, uint64_t b, bool c) {
    return a + b < a || (c && a + b == (uint64_t) -1);
  }

  bool check_hash(const crypto::hash &hash, difficulty_type difficulty) {
    uint64_t low, high, top, cur;
    // First check the highest word, this will most likely fail for a random hash.
    mul(swap64le(((const uint64_t *) &hash)[3]), difficulty, top, high);
    if (high != 0) {
      return false;
    }
    mul(swap64le(((const uint64_t *) &hash)[0]), difficulty, low, cur);
    mul(swap64le(((const uint64_t *) &hash)[1]), difficulty, low, high);
    bool carry = cadd(cur, low);
    cur = high;
    mul(swap64le(((const uint64_t *) &hash)[2]), difficulty, low, high);
    carry = cadc(cur, low, carry);
    carry = cadc(high, top, carry);
    return !carry;
  }

  difficulty_type next_difficulty(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {

    if(timestamps.size() > DIFFICULTY_WINDOW)
    {
      timestamps.resize(DIFFICULTY_WINDOW);
      cumulative_difficulties.resize(DIFFICULTY_WINDOW);
    }


    size_t length = timestamps.size();
    assert(length == cumulative_difficulties.size());
    if (length <= 1) {
      return 1;
    }
    static_assert(DIFFICULTY_WINDOW >= 2, "Window is too small");
    assert(length <= DIFFICULTY_WINDOW);
    sort(timestamps.begin(), timestamps.end());
    size_t cut_begin, cut_end;
    static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Cut length is too large");
    if (length <= DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) {
      cut_begin = 0;
      cut_end = length;
    } else {
      cut_begin = (length - (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) + 1) / 2;
      cut_end = cut_begin + (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT);
    }
    assert(/*cut_begin >= 0 &&*/ cut_begin + 2 <= cut_end && cut_end <= length);
    uint64_t time_span = timestamps[cut_end - 1] - timestamps[cut_begin];
    if (time_span == 0) {
      time_span = 1;
    }
    difficulty_type total_work = cumulative_difficulties[cut_end - 1] - cumulative_difficulties[cut_begin];
    assert(total_work > 0);
    uint64_t low, high;
    mul(total_work, target_seconds, low, high);
    // blockchain errors "difficulty overhead" if this function returns zero.
    // TODO: consider throwing an exception instead
    if (high != 0 || low + time_span - 1 < low) {
      return 0;
    }
    return (low + time_span - 1) / time_span;
  }

  // LWMA difficulty algorithm
  // Copyright (c) 2017-2018 Zawy
  // MIT license http://www.opensource.org/licenses/mit-license.php.
  // Tom Harding, Karbowanec, Masari, Bitcoin Gold, and Bitcoin Candy have contributed.
  // https://github.com/zawy12/difficulty-algorithms/issues/3
  // Implementation based on the Masari Project:
  // https://github.com/masari-project/masari/blob/master/src/cryptonote_basic/difficulty.cpp
  //
  // Graft Settings: adjust = 0.9909
  difficulty_type next_difficulty_v8(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {

    if (timestamps.size() > DIFFICULTY_BLOCKS_COUNT_V8) {
      timestamps.resize(DIFFICULTY_BLOCKS_COUNT_V8);
      cumulative_difficulties.resize(DIFFICULTY_BLOCKS_COUNT_V8);
    }

    size_t length = timestamps.size();
    assert(length == cumulative_difficulties.size());
    if (length <= 1) {
      return 1;
    }

    uint64_t weighted_timespans = 0;
    uint64_t target;

    uint64_t previous_max = timestamps[0];
    for (size_t i = 1; i < length; i++) {
      uint64_t timespan;
      uint64_t max_timestamp;

      if (timestamps[i] > previous_max) {
        max_timestamp = timestamps[i];
      } else {
        max_timestamp = previous_max;
      }

      timespan = max_timestamp - previous_max;
      if (timespan == 0) {
        timespan = 1;
      } else if (timespan > 10 * target_seconds) {
        timespan = 10 * target_seconds;
      }

      weighted_timespans += i * timespan;
      previous_max = max_timestamp;
    }
    // adjust = 0.99 for N=60, leaving the + 1 for now as it's not affecting N
    target = 0.9909 * (((length + 1) / 2) * target_seconds);

    uint64_t minimum_timespan = target_seconds * length / 2;
    if (weighted_timespans < minimum_timespan) {
      weighted_timespans = minimum_timespan;
    }

    difficulty_type total_work = cumulative_difficulties.back() - cumulative_difficulties.front();
    assert(total_work > 0);

    uint64_t low, high;
    mul(total_work, target, low, high);
    if (high != 0) {
      return 0;
    }
    uint64_t result =  low / weighted_timespans;
    return result > 0 ? result : 1;
  }

  difficulty_type next_difficulty_v9(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {

    if (timestamps.size() > DIFFICULTY_BLOCKS_COUNT_V8) {
      timestamps.resize(DIFFICULTY_BLOCKS_COUNT_V8);
      cumulative_difficulties.resize(DIFFICULTY_BLOCKS_COUNT_V8);
    }

    size_t length = timestamps.size();
    assert(length == cumulative_difficulties.size());
    if (length <= 1) {
      return 1;
    }

    uint64_t weighted_timespans = 0;
    uint64_t target;

    uint64_t previous_max = timestamps[0];
    for (size_t i = 1; i < length; i++) {
      uint64_t timespan;
      uint64_t max_timestamp;

      if (timestamps[i] > previous_max) {
        max_timestamp = timestamps[i];
      } else {
        max_timestamp = previous_max;
      }

      timespan = max_timestamp - previous_max;
      if (timespan == 0) {
        timespan = 1;
      } else if (timespan > 10 * target_seconds) {
        timespan = 10 * target_seconds;
      }

      weighted_timespans += i * timespan;
      previous_max = max_timestamp;
    }

    double derivative = 0;
    if (length >= 4 && timestamps[length - 1] - timestamps[length - 3] > 0) {
      double d_last = 1.0 * (cumulative_difficulties[length - 1] - cumulative_difficulties[length - 2]);
      double d_prev = 1.0 * (cumulative_difficulties[length - 3] - cumulative_difficulties[length - 4]);
      double h = 1.0 * (timestamps[length - 1] - timestamps[0]) / timestamps.size();
      if (h > 0) {
        derivative = (d_last - d_prev) / h;
      }
    }
    // adjust = 0.99 for N=60, leaving the + 1 for now as it's not affecting N
    double adjust = 0.9909;
    if (derivative < 0) {
      adjust *= 1 + std::atan(derivative) / (10 * M_PI);
    }
    target = adjust * (((length + 1) / 2) * target_seconds);

    uint64_t minimum_timespan = target_seconds * length / 2;
    if (weighted_timespans < minimum_timespan) {
      weighted_timespans = minimum_timespan;
    }

    difficulty_type total_work = cumulative_difficulties.back() - cumulative_difficulties.front();
    assert(total_work > 0);

    uint64_t low, high;
    mul(total_work, target, low, high);
    if (high != 0) {
      return 0;
    }
    uint64_t result =  low / weighted_timespans;
    return result > 0 ? result : 1;
  }

}
