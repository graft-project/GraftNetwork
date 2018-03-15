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

#include "TxPool.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/http_abstract_invoke.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

#include <exception>

using namespace std;

namespace supernode {

TxPool::TxPool(const std::string &daemon_addr, const std::string &daemon_login, const std::string &daemon_pass)
  : m_rpc_timeout(std::chrono::seconds(30))
{
    bool result = false;
    boost::optional<epee::net_utils::http::login> login{};
    if (!daemon_login.empty() && !daemon_pass.empty())
    {
        login.emplace(daemon_login, daemon_pass);
    }
    result = init(daemon_addr, login);
    if (!result)
    {
        throw std::runtime_error("can't connect to node: " + daemon_addr);
    }
}

TxPool::~TxPool()
{

}

bool TxPool::get(const string &hash_str, cryptonote::transaction &out_tx)
{
    crypto::hash hash;
    if (!epee::string_tools::hex_to_pod(hash_str, hash))
    {
        LOG_ERROR("error parsing input hash");
        return false;
    }

    // get the pool state
    cryptonote::COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::request req;
    cryptonote::COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::response res;

    bool r = epee::net_utils::invoke_http_json("/get_transaction_pool_hashes.bin", req, res, m_http_client, m_rpc_timeout);
    if (!r)
    {
        LOG_ERROR("/get_transaction_pool_hashes.bin error");
        return r;
    }

    MDEBUG("got pool");
    const auto it = std::find(res.tx_hashes.begin(), res.tx_hashes.end(), hash);
    if (it == res.tx_hashes.end())
    {
        MWARNING("tx: " << hash_str << " was not found in pool");
        return false;
    }

    // get full tx
    cryptonote::COMMAND_RPC_GET_TRANSACTIONS::request req_tx;
    cryptonote::COMMAND_RPC_GET_TRANSACTIONS::response res_tx;
    req_tx.txs_hashes.push_back(hash_str);

    req_tx.decode_as_json = false;
    r = epee::net_utils::invoke_http_json("/gettransactions", req_tx, res_tx, m_http_client, m_rpc_timeout);
    if (!r && res.status != CORE_RPC_STATUS_OK)
    {
        LOG_ERROR("/getransactions error");
        return false;
    }

    // parse transaction
    if (res_tx.txs.size() != 1)
    {
        LOG_ERROR("Wrong number of tx returned: " << res_tx.txs.size());
        return false;
    }

    cryptonote::blobdata bd;
    crypto::hash tx_hash, tx_prefix_hash;
    if (!epee::string_tools::parse_hexstr_to_binbuff(res_tx.txs[0].as_hex, bd))
    {
        LOG_ERROR("failed to parse tx from hex");
        return false;
    }

    if (!cryptonote::parse_and_validate_tx_from_blob(bd, out_tx, tx_hash, tx_prefix_hash))
    {
        LOG_ERROR("failed to parse tx from blob");
        return false;
    }

    if (hash != tx_hash)
    {
        LOG_ERROR("wrong tx received from daemon");
        return false;
    }
    return true;
}

bool TxPool::init(const string &daemon_address, boost::optional<epee::net_utils::http::login> daemon_login)
{
    return m_http_client.set_server(daemon_address, daemon_login);
    return false;
}

} // namespace
