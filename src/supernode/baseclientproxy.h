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


protected:
    void Init() override;

    bool GetWalletBalance(const rpc_command::GET_WALLET_BALANCE::request &in, rpc_command::GET_WALLET_BALANCE::response &out);

    bool CreateAccount(const rpc_command::CREATE_ACCOUNT::request &in, rpc_command::CREATE_ACCOUNT::response &out);
    bool GetSeed(const rpc_command::GET_SEED::request &in, rpc_command::GET_SEED::response &out);
    bool RestoreAccount(const rpc_command::RESTORE_ACCOUNT::request &in, rpc_command::RESTORE_ACCOUNT::response &out);


protected:

};

}

#endif // BASECLIENTPROXY_H
