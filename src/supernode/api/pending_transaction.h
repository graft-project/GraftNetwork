// Copyright (c) 2017, The Graft Project
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
// Parts of this file are originally copyright (c) 2014-2017, The Monero Project

#include "wallet/wallet2_api.h"
#include "../graft_wallet2.h"

#include <string>
#include <vector>


namespace Monero {

class GraftPendingTransactionImpl : public PendingTransaction
{
public:
    GraftPendingTransactionImpl(tools::GraftWallet2 *graft_wallet);
    ~GraftPendingTransactionImpl();
    int status() const;
    std::string errorString() const;
    bool commit(const std::string &filename = "", bool overwrite = false);
    uint64_t amount() const;
    uint64_t dust() const;
    uint64_t fee() const;
    std::vector<std::string> txid() const;
    uint64_t txCount() const;
    bool save(std::ostream &os);

    std::vector<std::string> getRawTransaction() const override;
    // TODO: continue with interface;
    void setPendingTx(std::vector<tools::GraftWallet2::pending_tx> pending_tx);
    void setStatus(int status);
    void setErrorString(const std::string &message);

private:
    tools::GraftWallet2 *mWallet;

    int  m_status;
    std::string m_errorString;
    std::vector<tools::GraftWallet2::pending_tx> m_pending_tx;
};


}

namespace Bitmonero = Monero;
