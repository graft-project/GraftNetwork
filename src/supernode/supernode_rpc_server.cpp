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
// Parts of this file are originally copyright (c) 2014-2017 The Monero Project

#include <boost/asio/ip/address.hpp>
#include <boost/filesystem/operations.hpp>
#include <cstdint>
#include "include_base_utils.h"
#include "string_coding.h"
#include "rpc/rpc_args.h"
#include "common/i18n.h"
using namespace epee;

#include "supernode_rpc_server.h"
#include "wallet/wallet_rpc_server_error_codes.h"
#include "mnemonics/electrum-words.h"
#include "graft_wallet.h"

tools::supernode_rpc_server::supernode_rpc_server() {}
tools::supernode_rpc_server::~supernode_rpc_server() {}

bool tools::supernode_rpc_server::init(const boost::program_options::variables_map *vm)
{
    auto rpc_config = cryptonote::rpc_args::process(*vm);
    if (!rpc_config)
        return false;

    m_vm = vm;
    boost::optional<epee::net_utils::http::login> http_login{};
    std::string bind_port = command_line::get_arg(*m_vm, arg_rpc_bind_port);
    const bool disable_auth = command_line::get_arg(*m_vm, arg_disable_rpc_login);
    //    m_trusted_daemon = command_line::get_arg(*m_vm, arg_trusted_daemon);

    if (disable_auth)
    {
        if (rpc_config->login)
        {
            const cryptonote::rpc_args::descriptors arg{};
            LOG_ERROR(tr("Cannot specify --") << arg_disable_rpc_login.name << tr(" and --") << arg.rpc_login.name);
            return false;
        }
    }
    else // auth enabled
    {
        if (!rpc_config->login)
        {
            std::array<std::uint8_t, 16> rand_128bit{{}};
            crypto::rand(rand_128bit.size(), rand_128bit.data());
            http_login.emplace(
                        default_rpc_username,
                        string_encoding::base64_encode(rand_128bit.data(), rand_128bit.size())
                        );
        }
        else
        {
            http_login.emplace(
                        std::move(rpc_config->login->username), std::move(rpc_config->login->password).password()
                        );
        }
        assert(bool(http_login));

        std::string temp = "graft-supernode." + bind_port + ".login";
        const auto cookie = tools::create_private_file(temp);
        if (!cookie)
        {
            LOG_ERROR(tr("Failed to create file ") << temp << tr(". Check permissions or remove file"));
            return false;
        }
        //      rpc_login_filename.swap(temp); // nothrow guarantee destructor cleanup
        //      temp = rpc_login_filename;
        //      std::fputs(http_login->username.c_str(), cookie.get());
        //      std::fputc(':', cookie.get());
        //      std::fputs(http_login->password.c_str(), cookie.get());
        //      std::fflush(cookie.get());
        //      if (std::ferror(cookie.get()))
        //      {
        //        LOG_ERROR(tr("Error writing to file ") << temp);
        //        return false;
        //      }
        LOG_PRINT_L0(tr("RPC username/password is stored in file ") << temp);
    } // end auth enabled

    //    m_http_client.set_server(walvars->get_daemon_address(), walvars->get_daemon_login());

    m_net_server.set_threads_prefix("RPC");
    return epee::http_server_impl_base<supernode_rpc_server, connection_context>::init(
                std::move(bind_port), std::move(rpc_config->bind_ip), std::move(http_login)
              );
}

const char *tools::supernode_rpc_server::tr(const char *str)
{
    return i18n_translate(str, "tools::supernode_rpc_server");
}


bool tools::supernode_rpc_server::on_test_call(const supernode_rpc::COMMAND_RPC_EMPTY_TEST::request& req, supernode_rpc::COMMAND_RPC_EMPTY_TEST::response& res, epee::json_rpc::error& er) {
    LOG_PRINT_L0("\n\n--------------------------------- 1 get test call\n");
    sleep(5);
    LOG_PRINT_L0("\n\n--------------------------------- 2 get test call\n");
    return true;
}

bool tools::supernode_rpc_server::onReadyToPay(const tools::supernode_rpc::COMMAND_RPC_READY_TO_PAY::request &req, tools::supernode_rpc::COMMAND_RPC_READY_TO_PAY::response &res, json_rpc::error &er)
{
    return true;
}

bool tools::supernode_rpc_server::onRejectPay(const tools::supernode_rpc::COMMAND_RPC_REJECT_PAY::request &req, tools::supernode_rpc::COMMAND_RPC_REJECT_PAY::response &res, json_rpc::error &er)
{
    return true;
}

bool tools::supernode_rpc_server::onPay(const tools::supernode_rpc::COMMAND_RPC_PAY::request &req, tools::supernode_rpc::COMMAND_RPC_PAY::response &res, json_rpc::error &er)
{
    return true;
}

bool tools::supernode_rpc_server::onGetPayStatus(const tools::supernode_rpc::COMMAND_RPC_GET_PAY_STATUS::request &req, tools::supernode_rpc::COMMAND_RPC_GET_PAY_STATUS::response &res, json_rpc::error &er)
{
    return true;
}

bool tools::supernode_rpc_server::onSale(const tools::supernode_rpc::COMMAND_RPC_SALE::request &req, tools::supernode_rpc::COMMAND_RPC_SALE::response &res, json_rpc::error &er)
{
    return true;
}

bool tools::supernode_rpc_server::onGetSaleStatus(const tools::supernode_rpc::COMMAND_RPC_GET_SALE_STATUS::request &req, tools::supernode_rpc::COMMAND_RPC_GET_SALE_STATUS::response &res, json_rpc::error &er)
{
    return true;
}

bool tools::supernode_rpc_server::onGetWalletBalance(const tools::supernode_rpc::COMMAND_RPC_GET_WALLET_BALANCE::request &req, tools::supernode_rpc::COMMAND_RPC_GET_WALLET_BALANCE::response &res, json_rpc::error &er)
{
    namespace po = boost::program_options;
    po::variables_map vm2;
    {
      po::options_description desc("dummy");
      const command_line::arg_descriptor<std::string, true> arg_password = {"password", "password"};
      const char *argv[4];
      int argc = 3;
      argv[0] = "wallet-rpc";
      argv[1] = "--password";
      argv[2] = req.password.c_str();
      argv[3] = NULL;
      vm2 = *m_vm;
      command_line::add_arg(desc, arg_password);
      po::store(po::parse_command_line(argc, argv, desc), vm2);
    }
    std::unique_ptr<tools::GraftWallet> wal;
    try {
      wal = tools::GraftWallet::make_from_data(vm2, req.account).first;
    }
    catch (const std::exception& e)
    {
      wal = nullptr;
    }
    if (!wal)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "Failed to open wallet";
      return false;
    }

//      if (!wal) return not_open(er);
    try
    {
      res.balance = wal->balance();
      res.unlocked_balance = wal->unlocked_balance();
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
}

bool tools::supernode_rpc_server::onGetSupernodeList(const tools::supernode_rpc::COMMAND_RPC_GET_SUPERNODE_LIST::request &req, tools::supernode_rpc::COMMAND_RPC_GET_SUPERNODE_LIST::response &res, json_rpc::error &er)
{
    return true;
}

bool tools::supernode_rpc_server::onCreateAccount(const tools::supernode_rpc::COMMAND_RPC_CREATE_ACCOUNT::request &req, tools::supernode_rpc::COMMAND_RPC_CREATE_ACCOUNT::response &res, json_rpc::error &er)
{
    LOG_PRINT_L0("start create acoount");
    namespace po = boost::program_options;
    po::variables_map vm2;

    {
      std::vector<std::string> languages;
      crypto::ElectrumWords::get_language_list(languages);
      std::vector<std::string>::iterator it;

      it = std::find(languages.begin(), languages.end(), req.language);
      if (it == languages.end())
      {
        er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
        er.message = "Unknown language";
        return false;
      }
    }
    LOG_PRINT_L0("create acoount: step 1");
    {
      po::options_description desc("dummy");
      const command_line::arg_descriptor<std::string, true> arg_password = {"password", "password"};
      const char *argv[4];
      int argc = 3;
      argv[0] = "wallet-rpc";
      argv[1] = "--password";
      argv[2] = req.password.c_str();
      argv[3] = NULL;
      vm2 = *m_vm;
      command_line::add_arg(desc, arg_password);
      po::store(po::parse_command_line(argc, argv, desc), vm2);
    }
    LOG_PRINT_L0("create acoount: step 2");
    std::unique_ptr<tools::GraftWallet> wal = tools::GraftWallet::make_new(vm2).first;
    if (!wal)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "Failed to create wallet";
      return false;
    }
    LOG_PRINT_L0("create acoount: step 3");
    wal->set_seed_language(req.language);
    cryptonote::COMMAND_RPC_GET_HEIGHT::request hreq;
    cryptonote::COMMAND_RPC_GET_HEIGHT::response hres;
    hres.height = 0;
    bool r = net_utils::invoke_http_json("/getheight", hreq, hres, m_http_client);
    wal->set_refresh_from_block_height(hres.height);
    crypto::secret_key dummy_key;
    try {
      wal->generate_graft(req.password, dummy_key, false, false);
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "Failed to generate wallet";
      return false;
    }
    LOG_PRINT_L0("create acoount: step 4");
    res.account = wal->store_keys_graft(req.password);
    LOG_PRINT_L0(res.account);

///    if (m_wallet)
///      delete m_wallet;
///    m_wallet = wal.release();
    return true;
}



