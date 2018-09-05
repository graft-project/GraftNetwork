// Copyright (c) 2018, The Graft Project
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

#ifndef UTILS_H
#define UTILS_H

#include <ringct/rctSigs.h>
#include <cryptonote_basic/account.h>
#include <cryptonote_basic/cryptonote_basic.h>
#include <vector>
#include <atomic>
#include <random>
#include <algorithm>

namespace Utils {
/*!
 * \brief lookup_account_outputs_ringct - returns outputs owned by address in given tx
 * \param acc                - account to check
 * \param tx                 - transaction to check
 * \param outputs            - vector of pairs <output_index, amount>
 * \param total_transfered   - total amount addressed to given address in given transaction
 * \return                   - true on success
 */
bool lookup_account_outputs_ringct(const cryptonote::account_keys &acc, const cryptonote::transaction &tx,
                                   std::vector<std::pair<size_t, uint64_t>> &outputs, uint64_t &total_transfered);

template <typename T, typename ...Args>
T& create_once(Args... args)
{
    static std::atomic<T *> object {nullptr};

    if (object.load(std::memory_order_acquire))
        return *object.load(std::memory_order_relaxed);

    static std::atomic<bool> initializing {false};
    bool expected {false};

    if (initializing.compare_exchange_strong(expected, true))
        object = new T(args...);
    else
        while (!object);

    return *object;
}

class shuffler
{
    std::shared_ptr<std::mt19937> re;

public:
    shuffler()
    {
        std::random_device rd;
        re = std::make_shared<std::mt19937>(rd());
    }

    shuffler(const shuffler& s) : re(s.re) {}

    template <class RandomAccessContainer>
    void run(RandomAccessContainer& c)
    {
        return std::shuffle(c.begin(), c.end(), *re);
    }
};

} // namespace

#endif // UTILS_H
