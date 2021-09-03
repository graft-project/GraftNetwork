// Copyright (c) 2018, The Loki Project
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

#ifndef LOKI_H
#define LOKI_H

#define LOKI_HOUR(val) ((val) * LOKI_MINUTES(60))
#define LOKI_MINUTES(val) val * 60

#include <cstddef>
#include <cstdint>
#include <string>
#include <iterator>
#include <cassert>

#define LOKI_RPC_DOC_INTROSPECT
namespace loki
{
double      round           (double);
double      exp2            (double);

constexpr uint64_t clamp_u64(uint64_t val, uint64_t min, uint64_t max)
{
  assert(min <= max);
  if (val < min) val = min;
  else if (val > max) val = max;
  return val;
}

template <typename lambda_t>
struct deferred
{
private:
  lambda_t lambda;
  bool cancelled = false;
public:
  deferred(lambda_t lambda) : lambda(lambda) {}
  void cancel() { cancelled = true; }
  ~deferred() { if (!cancelled) lambda(); }
};

template <typename lambda_t>
#ifdef __GNUG__
[[gnu::warn_unused_result]]
#endif
deferred<lambda_t> defer(lambda_t lambda) { return lambda; }

struct defer_helper
{
  template <typename lambda_t>
  deferred<lambda_t> operator+(lambda_t lambda) { return lambda; }
};

#define LOKI_TOKEN_COMBINE2(x, y) x ## y
#define LOKI_TOKEN_COMBINE(x, y) LOKI_TOKEN_COMBINE2(x, y)
#define LOKI_DEFER auto const LOKI_TOKEN_COMBINE(loki_defer_, __LINE__) = loki::defer_helper() + [&]()

template <typename T, size_t N>
constexpr size_t array_count(T (&)[N]) { return N; }

template <typename T, size_t N>
constexpr size_t char_count(T (&)[N]) { return N - 1; }

}; // namespace Loki

#endif // LOKI_H
