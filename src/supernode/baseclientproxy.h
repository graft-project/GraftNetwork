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

#ifndef BASECLIENTPROXY_H
#define BASECLIENTPROXY_H

#include "BaseRTAProcessor.h"
#include "graft_wallet.h"

namespace supernode {
class BaseClientProxy : public BaseRTAProcessor
{
public:
    BaseClientProxy();

    std::unique_ptr<tools::GraftWallet> initWallet(const std::string &account, const std::string &password) const;
    void storeWalletState(tools::GraftWallet *wallet);

    static std::string base64_decode(const std::string &encoded_data);
    static std::string base64_encode(const std::string &data);

protected:
    void Init() override;

    bool GetWalletBalance(const rpc_command::GET_WALLET_BALANCE::request &in, rpc_command::GET_WALLET_BALANCE::response &out);

    bool CreateAccount(const rpc_command::CREATE_ACCOUNT::request &in, rpc_command::CREATE_ACCOUNT::response &out);
    bool GetSeed(const rpc_command::GET_SEED::request &in, rpc_command::GET_SEED::response &out);
    bool RestoreAccount(const rpc_command::RESTORE_ACCOUNT::request &in, rpc_command::RESTORE_ACCOUNT::response &out);

    bool GetTransferFee(const rpc_command::GET_TRANSFER_FEE::request &in, rpc_command::GET_TRANSFER_FEE::response &out);
    bool Transfer(const rpc_command::TRANSFER::request &in, rpc_command::TRANSFER::response &out);

private:
    bool validate_transfer(tools::GraftWallet *wallet,
                           const std::string &address, uint64_t amount,
                           const std::string payment_id,
                           std::vector<cryptonote::tx_destination_entry>& dsts,
                           std::vector<uint8_t>& extra);
};

}

#endif // BASECLIENTPROXY_H
