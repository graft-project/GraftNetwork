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

#include "supernode_rpc_server.h"
#include "wallet/wallet_args.h"
#include "rpc/rpc_args.h"

int main(int argc, char** argv) {
    namespace po = boost::program_options;

    po::options_description desc_params(wallet_args::tr("Supernode options"));
    command_line::add_arg(desc_params, arg_rpc_bind_port);
    command_line::add_arg(desc_params, arg_disable_rpc_login);
    command_line::add_arg(desc_params, arg_trusted_daemon);
    cryptonote::rpc_args::init_options(desc_params);

    //      --daemon-address arg            Use daemon instance at <host>:<port>
    //      --daemon-host arg               Use daemon instance at host <arg> instead of
    //                                      localhost
    //      --daemon-port arg (=0)          Use daemon instance at port <arg> instead of
    //                                      18081
    //      --daemon-login arg              Specify username[:password] for daemon RPC
    //                                      client
    //      --testnet                       For testnet. Daemon must also be launched
    //                                      with --testnet flag
    //      --restricted-rpc                Restricts to view-only commands
    //      --log-file arg                  Specify log file
    //      --log-level arg                 0-4 or categories
    //      --max-concurrency arg (=0)      Max number of threads to use for a parallel
    //                                      job
    //      --config-file arg               Config file

    const auto vm = wallet_args::main(
                argc, argv,
                "graft-supernode [--rpc-bind-port=<port>]",
                desc_params,
                po::positional_options_description(),
                "graft-supernode.log",
                true
                );
    if (!vm)
    {
        return 1;
    }

    LOG_PRINT_L0(tools::supernode_rpc_server::tr("Initializing Graft Supernode Server..."));
    tools::supernode_rpc_server rpc;
//        rpc.init( "7655", "127.0.0.1");
    bool r = rpc.init(&(vm.get()));
    CHECK_AND_ASSERT_MES(r, 1, tools::supernode_rpc_server::tr("Failed to initialize Graft Supernode Server!"));
    LOG_PRINT_L0(tools::supernode_rpc_server::tr("Starting Graft Supernode Server..."));
    rpc.run(100);
    LOG_PRINT_L0(tools::supernode_rpc_server::tr("Stopped Graft Supernode Server!"));

    return 0;
}



