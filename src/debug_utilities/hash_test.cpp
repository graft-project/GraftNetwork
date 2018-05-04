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


#include <cryptonote_core/cryptonote_core.h>
#include <crypto/hash.h>
#include <profile_tools.h>
#include <string>
#include <iostream>
#include <boost/program_options.hpp>

using namespace std;


namespace po = boost::program_options;

uint64_t hash_profile(const char * data, size_t len, size_t count, int variant)
{
    crypto::hash h;
    TIME_MEASURE_START(ts);

    for (size_t i = 0; i < count; ++i) {
        crypto::cn_slow_hash(data, len, h, variant);
    }

    TIME_MEASURE_FINISH(ts);
    return ts;
}

void benchmark()
{
    size_t count = 200;
    cryptonote::block b;
    cryptonote::blobdata bd = get_block_hashing_blob(b);

    for (int variant = 0; variant < 3; variant++) {
        LOG_PRINT_L0("Benchmarking v" << variant << " " << count  << " times...");
        uint64_t elapsed = hash_profile(bd.data(), bd.size(), count, variant);
        LOG_PRINT_L0("... " << elapsed << " ms" << ", " << (double)count/elapsed * 1000 << " H/s" );
    }
}


int main(int argc, char **argv)
{

    string input;
    int log_level;
    int variant;

    try {
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")
                ("slow-hash-variant",  po::value<int>(&variant), "slow hash variant: 0 - monero v1, 1 - monero v7, 2 - cryptonote heavy")
                ("input",  po::value<string>(), "input string")
                ("log-level",       po::value<int>(&log_level)->default_value(1), "log-level");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        po::notify(vm);


        if (vm.count("help") || !vm.count("slow-hash-variant") || !vm.count("input")) {
            cout << desc << "\n";
            return 0;
        }


        if (vm.count("log-level")) {
            log_level = vm["log-level"].as<int>();
        }

        mlog_configure("", true);
        mlog_set_log_level(log_level);


        variant = vm["slow-hash-variant"].as<int>();
        input  = vm["input"].as<string>();

    } catch (...) {
        cerr << "Exception thrown..";
        return -1;
    }

    crypto::hash h;
    crypto::cn_slow_hash(input.data(), input.size(), h, variant);

    std::cout << epee::string_tools::pod_to_hex(h) << std::endl;

    return 0;

}
