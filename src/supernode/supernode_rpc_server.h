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

#pragma	once

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <string>
#include "supernode_rpc_server_commands_defs.h"
#include "net/http_server_impl_base.h"
#include "common/command_line.h"

namespace
{
const command_line::arg_descriptor<std::string, true> arg_rpc_bind_port = {"rpc-bind-port", "Sets bind port for server"};
const command_line::arg_descriptor<bool> arg_disable_rpc_login = {"disable-rpc-login", "Disable HTTP authentication for RPC connections served by this process"};
const command_line::arg_descriptor<bool> arg_trusted_daemon = {"trusted-daemon", "Enable commands which rely on a trusted daemon", false};

constexpr const char default_rpc_username[] = "graft";
}

namespace tools {
class supernode_rpc_server: public epee::http_server_impl_base<supernode_rpc_server> {
public:
    typedef epee::net_utils::connection_context_base connection_context;

public:
    supernode_rpc_server();
    ~supernode_rpc_server();

    bool init(const boost::program_options::variables_map *vm);

    static const char* tr(const char* str);

private:
    CHAIN_HTTP_TO_MAP2(connection_context) //forward http requests to uri map

    BEGIN_URI_MAP2()
    BEGIN_JSON_RPC_MAP("/json_rpc")
    MAP_JON_RPC_WE("test_call",         on_test_call,       supernode_rpc::COMMAND_RPC_EMPTY_TEST)
    END_JSON_RPC_MAP()
    END_URI_MAP2()

    //json_rpc
    bool on_test_call(const supernode_rpc::COMMAND_RPC_EMPTY_TEST::request& req, supernode_rpc::COMMAND_RPC_EMPTY_TEST::response& res, epee::json_rpc::error& er);

private:
    bool m_trusted_daemon;
    std::string rpc_login_filename;
    const boost::program_options::variables_map *m_vm;
};
}
