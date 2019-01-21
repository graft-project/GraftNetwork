// Copyright (c) 2018, The Graft Project
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

#include "cryptonote_core/cryptonote_core.h"
#include "crypto/hash.h"
#include "net/levin_client.h"
#include "common/int-util.h"

#include "p2p/p2p_protocol_defs.h"
#include "storages/portable_storage_template_helper.h"
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <string>

#include <iostream>
#include <functional>

using namespace std;
namespace po = boost::program_options;

using namespace cryptonote;

int main(int argc, char **argv)
{
    int log_level = 1;
    std::string target_addr;
    size_t repeat_count = 1;
    size_t workers_count = 1;
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")
                ("target",    po::value<string>()->required(), "target host")
                ("count",     po::value<size_t>(&repeat_count)->default_value(1), "repeat count")
                ("workers",   po::value<size_t>(&workers_count)->default_value(1), "workers count")
                ("log-level", po::value<int>(&log_level)->default_value(1), "log-level");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        po::notify(vm);


        if (vm.count("help")) {
            cout << desc << "\n";
            return 0;
        }


        if (vm.count("log-level")) {
            log_level = vm["log-level"].as<int>();
        }

        mlog_configure("", true);
        mlog_set_log_level(log_level);

        if (vm.count("target")) {
            target_addr = vm["target"].as<string>();
        }

        if (vm.count("count")) {
            repeat_count = vm["count"].as<size_t>();
        }

        if (vm.count("workers")) {
            workers_count = vm["workers"].as<size_t>();
        }

        // TODO: fill announce
        // how to serialize?
        std::vector<epee::net_utils::levin_client*> clients;
        std::vector<std::thread> workers;

        auto worker_func = [&](epee::net_utils::levin_client * client, size_t repeat_count) {
            MDEBUG("client: " << client << ", repeats: " << repeat_count);

            if (!client) {
                client = new epee::net_utils::levin_client();
                bool conn_result = client->connect(target_addr, 28680, 1000);
                MDEBUG("client connect result: " << conn_result);
                MDEBUG("client connected: " << client->is_connected());
                if (!client->is_connected()) {
                    MERROR("Failed to connect");
                    delete client;
                    return;
                }
            }
            for (size_t i = 0; i < repeat_count; ++i) {
                nodetool::COMMAND_BROADCAST::request request;
                request.sender_address = "F3uaEkMNLgHQ8GF34QSusHa3kDsySG3iXL9nmArEcehP7qz3c6trqpqJZkb6YcVGQVRJU3KSczDo4XPf7HPc3oWPARiphdp";
                boost::uuids::uuid id;
                request.message_id = to_string(id);
                request.callback_uri = "/debug/auth_sample/10000";
                request.hop = 1;
                request.data = "";
                string buf = epee::serialization::store_t_to_binary(request);
                MDEBUG("sending broadcast: " << i);
                client->notify(nodetool::COMMAND_BROADCAST::ID, buf);
            }
        };


        MINFO("starting...");
        for (size_t i = 0; i < workers_count; ++i) {
            epee::net_utils::levin_client * levin_client = nullptr;
            workers.push_back(std::thread(worker_func, levin_client, repeat_count));
        }

        for (auto & worker : workers) {
            worker.join();
        }

        MINFO("All workers done");
        // TODO: here's a memleak with the levin_client objects but this is ok for the purpose of the test

    } catch (const std::exception &e) {
        // close() syncs db which throwns an exception as db opened in read-only mode
        cerr << "exception thrown: " << e.what() << endl;
    } catch (...) {
        cerr << "Unhandled/unknown exception thrown\n";
    }

    return 0;
}

