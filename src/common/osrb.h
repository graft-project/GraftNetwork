// Copyright (c)      2019, The Loki Project
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
#include <streambuf>
#include <string>

namespace tools {

/// Simple class to read from memory in-place.  Intended use:
///
///     one_shot_read_buffer buf{data, len};
///     std::istream is{&buf};
///     is >> foo; /* do some istream stuff with is */
///
class one_shot_read_buffer : public std::stringbuf {
public:
    /// Construct from char pointer & size
    one_shot_read_buffer(const char *s_in, size_t n) : std::stringbuf(std::ios::in) {
        // We won't actually modify it, but setg needs non-const
        auto *s = const_cast<char *>(s_in);
        setg(s, s, s+n);
    }

    /// Construct from std::string lvalue reference (but *not* a temporary, see below!)
    explicit one_shot_read_buffer(const std::string &s_in)
        : one_shot_read_buffer{s_in.data(), s_in.size()} {}

    /// Explicitly disallow construction with std::string temporary
    explicit one_shot_read_buffer(const std::string &&s) = delete;
};

}
