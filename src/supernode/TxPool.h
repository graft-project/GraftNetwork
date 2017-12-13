#ifndef TXPOOL_H
#define TXPOOL_H

#include <string>
#include <vector>
#include <chrono>
#include <boost/optional.hpp>

#include "net/http_client.h"
#include "net/http_auth.h"
#include "cryptonote_basic/cryptonote_basic.h"


namespace supernode {

class TxPool
{
public:
    TxPool(const std::string &daemon_addr, const std::string &daemon_login, const std::string &daemon_pass);
    virtual ~TxPool();
    bool get(const std::string &hash_str, cryptonote::transaction &out_tx);


protected:
    bool init(const std::string &daemon_address, boost::optional<epee::net_utils::http::login> daemon_login);


private:
    epee::net_utils::http::http_simple_client m_http_client;
    static constexpr const std::chrono::seconds m_rpc_timeout = std::chrono::seconds(30);

};

}

#endif // TXPOOL_H
