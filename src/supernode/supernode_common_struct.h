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

#ifndef SUPERNODE_COMMON_STRUCT_H_
#define SUPERNODE_COMMON_STRUCT_H_

#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>
#include <inttypes.h>
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "crypto/hash.h"
using namespace std;

namespace supernode {

enum class NTransactionStatus : int
{
    None = 0,
    InProgress=1,
    Success=2,
    Fail=3,
    RejectedByWallet=4,
    RejectedByPOS=5
};

struct FSN_WalletData
{
    FSN_WalletData() = default;
    FSN_WalletData(const FSN_WalletData &other) = default;
    FSN_WalletData(const std::string &_addr, const std::string &_viewkey)
        : Addr{_addr}
        , ViewKey{_viewkey} {}

    bool operator!=(const FSN_WalletData& s) const;
    bool operator==(const FSN_WalletData& s) const;

    string Addr;
    string ViewKey;
};

struct FSN_Data
{
    FSN_Data(const FSN_WalletData &_stakeWallet, const FSN_WalletData &_minerWallet,
             const std::string &_ip = "", const std::string &_port = "")
        : Stake{_stakeWallet}
        , Miner{_minerWallet}
        , IP{_ip}
        , Port{_port} {}
    FSN_Data() {}

    bool operator!=(const FSN_Data& s) const;
    bool operator==(const FSN_Data& s) const;
    FSN_WalletData Stake;
    FSN_WalletData Miner;
    string IP;
    string Port;
};

struct SubNetData
{
    BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(PaymentID)
    END_KV_SERIALIZE_MAP()

    string PaymentID;
};

struct RTA_TransactionRecordBase : public SubNetData
{
    uint64_t Amount;
    string POSAddress;
    string POSViewKey;
    string POSSaleDetails;// empty in wallet call
    uint64_t BlockNum;// empty in pos call
};

struct RTA_TransactionRecord : public RTA_TransactionRecordBase
{
    bool operator!=(const RTA_TransactionRecord& s) const;
    bool operator==(const RTA_TransactionRecord& s) const;
    string MessageForSign() const;
    vector< boost::shared_ptr<FSN_Data> > AuthNodes;
};

}

#endif /* SUPERNODE_COMMON_STRUCT_H_ */
