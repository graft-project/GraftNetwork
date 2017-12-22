#include "mnemonics/electrum-words.h"
#include "common/command_line.h"
#include "baseclientproxy.h"
#include "graft_defines.h"

supernode::BaseClientProxy::BaseClientProxy()
{
}

void supernode::BaseClientProxy::Init()
{
    m_DAPIServer->ADD_DAPI_HANDLER(GetWalletBalance, rpc_command::GET_WALLET_BALANCE, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(CreateAccount, rpc_command::CREATE_ACCOUNT, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(GetSeed, rpc_command::GET_SEED, BaseClientProxy);
    m_DAPIServer->ADD_DAPI_HANDLER(RestoreAccount, rpc_command::RESTORE_ACCOUNT, BaseClientProxy);
}

bool supernode::BaseClientProxy::GetWalletBalance(const supernode::rpc_command::GET_WALLET_BALANCE::request &in, supernode::rpc_command::GET_WALLET_BALANCE::response &out)
{
    std::unique_ptr<tools::GraftWallet> wal = initWallet(in.Account, in.Password);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    try
    {
        out.Balance = wal->balance();
        out.UnlockedBalance = wal->unlocked_balance();
    }
    catch (const std::exception& e)
    {
        out.Result = ERROR_BALANCE_NOT_AVAILABLE;
        return false;
    }
    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::CreateAccount(const supernode::rpc_command::CREATE_ACCOUNT::request &in, supernode::rpc_command::CREATE_ACCOUNT::response &out)
{
    std::vector<std::string> languages;
    crypto::ElectrumWords::get_language_list(languages);
    std::vector<std::string>::iterator it;
    it = std::find(languages.begin(), languages.end(), in.Language);
    if (it == languages.end())
    {
        out.Result = ERROR_LANGUAGE_IS_NOT_FOUND;
        return false;
    }

    std::unique_ptr<tools::GraftWallet> wal =
            tools::GraftWallet::createWallet(std::string(), m_Servant->GetNodeIp(), m_Servant->GetNodePort(),
                                             m_Servant->GetNodeLogin(), m_Servant->IsTestnet());
    if (!wal)
    {
        out.Result = ERROR_CREATE_WALLET_FAILED;
        return false;
    }
    wal->set_seed_language(in.Language);
    wal->set_refresh_from_block_height(m_Servant->GetCurrentBlockHeight());
    crypto::secret_key dummy_key;
    try
    {
        wal->generate_graft(in.Password, dummy_key, false, false);
    }
    catch (const std::exception& e)
    {
        out.Result = ERROR_CREATE_WALLET_FAILED;
        return false;
    }
    out.Account = wal->store_keys_graft(in.Password);
    out.Address = wal->get_account().get_public_address_str(wal->testnet());
    out.ViewKey = epee::string_tools::pod_to_hex(
                wal->get_account().get_keys().m_account_address.m_view_public_key);
    std::string seed;
    wal->get_seed(seed);
    out.Seed = seed;
    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::GetSeed(const supernode::rpc_command::GET_SEED::request &in, supernode::rpc_command::GET_SEED::response &out)
{
    std::unique_ptr<tools::GraftWallet> wal = initWallet(in.Account, in.Password);
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    wal->set_seed_language(in.Language);
    std::string seed;
    wal->get_seed(seed);
    out.Seed = seed;
    out.Result = STATUS_OK;
    return true;
}

bool supernode::BaseClientProxy::RestoreAccount(const supernode::rpc_command::RESTORE_ACCOUNT::request &in, supernode::rpc_command::RESTORE_ACCOUNT::response &out)
{
    if (in.Seed.empty())
    {
        out.Result = ERROR_ELECTRUM_SEED_EMPTY;
        return false;
    }
    crypto::secret_key recovery_key;
    std::string old_language;
    if (!crypto::ElectrumWords::words_to_bytes(in.Seed, recovery_key, old_language))
    {
        out.Result = ERROR_ELECTRUM_SEED_INVALID;
        return false;
    }
    std::unique_ptr<tools::GraftWallet> wal =
        tools::GraftWallet::createWallet(std::string(), m_Servant->GetNodeIp(), m_Servant->GetNodePort(),
                                             m_Servant->GetNodeLogin(), m_Servant->IsTestnet());
    if (!wal)
    {
        out.Result = ERROR_OPEN_WALLET_FAILED;
        return false;
    }
    try
    {
        wal->set_seed_language(old_language);
        wal->generate_graft(in.Password, recovery_key, true, false);
        out.Account = wal->store_keys_graft(in.Password);
        out.Address = wal->get_account().get_public_address_str(wal->testnet());
        out.ViewKey = epee::string_tools::pod_to_hex(
                    wal->get_account().get_keys().m_account_address.m_view_public_key);
        std::string seed;
        wal->get_seed(seed);
        out.Seed = seed;
    }
    catch (const std::exception &e)
    {
        out.Result = ERROR_RESTORE_WALLET_FAILED;
        return false;
    }
    out.Result = STATUS_OK;
    return true;
}

std::unique_ptr<tools::GraftWallet> supernode::BaseClientProxy::initWallet(const string &account, const string &password) const
{
    std::unique_ptr<tools::GraftWallet> wal;
    try
    {
        wal = tools::GraftWallet::createWallet(account, password, "",
                                               m_Servant->GetNodeIp(), m_Servant->GetNodePort(),
                                         m_Servant->GetNodeLogin(), m_Servant->IsTestnet());

    }
    catch (const std::exception& e)
    {
        wal = nullptr;
    }
    return wal;
}
