#include "TxPool.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/http_abstract_invoke.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

#include <exception>

using namespace std;

namespace supernode {

TxPool::TxPool(const std::string &daemon_addr, const std::string &daemon_login, const std::string &daemon_pass)
{

    bool result = false;
    boost::optional<epee::net_utils::http::login> login{};
    if (!daemon_login.empty() && !daemon_pass.empty()) {
        login.emplace(daemon_login, daemon_pass);
    }
    result = init(daemon_addr, login);
    if (!result) {
        throw std::runtime_error("can't connect to node: " + daemon_addr);
    }
}

bool TxPool::get(const string &hash_str, cryptonote::transaction &out_tx)
{
    crypto::hash hash;
    if (!epee::string_tools::hex_to_pod(hash_str, hash)) {
        LOG_ERROR("error parsing input hash");
        return false;
    }

    // get the pool state
    cryptonote::COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::request req;
    cryptonote::COMMAND_RPC_GET_TRANSACTION_POOL_HASHES::response res;

    bool r = epee::net_utils::invoke_http_json("/get_transaction_pool_hashes.bin", req, res, m_http_client, m_rpc_timeout);
    if (!r) {
        LOG_ERROR("/get_transaction_pool_hashes.bin error");
        return r;
    }

    MDEBUG("got pool");
    const auto it = std::find(res.tx_hashes.begin(), res.tx_hashes.end(), hash);
    if (it == res.tx_hashes.end()) {
       MWARNING("tx: " << hash_str << " was not found in pool");
       return false;
    }

    // get full tx
    cryptonote::COMMAND_RPC_GET_TRANSACTIONS::request req_tx;
    cryptonote::COMMAND_RPC_GET_TRANSACTIONS::response res_tx;
    req_tx.txs_hashes.push_back(hash_str);

    req_tx.decode_as_json = false;
    r = epee::net_utils::invoke_http_json("/gettransactions", req_tx, res_tx, m_http_client, m_rpc_timeout);
    if (!r && res.status != CORE_RPC_STATUS_OK) {
        LOG_ERROR("/getransactions error");
        return false;
    }

    // parse transaction
    if (res_tx.txs.size() != 1) {
        LOG_ERROR("Wrong number of tx returned: " << res_tx.txs.size());
        return false;
    }

    cryptonote::blobdata bd;
    crypto::hash tx_hash, tx_prefix_hash;
    if (!epee::string_tools::parse_hexstr_to_binbuff(res_tx.txs[0].as_hex, bd)) {
        LOG_ERROR("failed to parse tx from hex");
        return false;
    }

    if (!cryptonote::parse_and_validate_tx_from_blob(bd, out_tx, tx_hash, tx_prefix_hash)) {
        LOG_ERROR("failed to parse tx from blob");
        return false;
    }

    if (hash != tx_hash) {
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
