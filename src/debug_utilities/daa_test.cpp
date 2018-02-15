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
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <string>

#include <iostream>
using namespace std;
namespace po = boost::program_options;


int main(int argc, char **argv)
{

    string bdb_path;
    string output_file;

    int    log_level;

    uint64_t start_block = 0;
    uint64_t end_block   = 0;

    try {
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")
                ("bdb-path",        po::value<string>()->required(), "path to blockchain db")
                ("output-file",     po::value<string>()->required(), "output file")
                ("start-block",     po::value<uint64_t>(&start_block)->required(), "start block")
                ("end-block",       po::value<uint64_t>(&end_block)->required(), "end block")
                ("log-level",       po::value<int>(&log_level)->default_value(0), "log-level");

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

        bdb_path = vm["bdb-path"].as<string>();
        output_file = vm["output-file"].as<string>();
        start_block = vm["start-block"].as<uint64_t>();
        end_block   = vm["end-block"].as<uint64_t>();

        ofstream output(output_file);

        cryptonote::BlockchainDB *bdb = cryptonote::new_db("lmdb");

        if (!bdb) {
            LOG_ERROR("Error initializing blockchain db");
            // TODO: set status
            return -1;
        }

        boost::filesystem::path folder(bdb_path);
        folder = boost::filesystem::canonical(folder);

        folder /= bdb->get_db_name();
        const std::string filename = folder.string();
        LOG_PRINT_L0("Loading blockchain from folder " << filename << " ...");
        try
        {
            bdb->open(filename, DBF_RDONLY | DBF_FAST );
        }
        catch (const std::exception& e)
        {
            LOG_PRINT_L0("Error opening database: " << e.what());
            return -1;
        }

        for (uint64_t h = start_block; h <= end_block; ++h) {
            cryptonote::difficulty_type cum_difficulty = bdb->get_block_cumulative_difficulty(h);
            cryptonote::difficulty_type difficulty = bdb->get_block_difficulty(h);
            uint64_t timestamp = bdb->get_block_timestamp(h);
            output << h << ", " << timestamp << ", "  << difficulty << ", " << cum_difficulty << endl;
        }

        try {
            bdb->close();
        } catch (...) {
            // exception thrown while closing read-only opened db as it tries to sync it
        }

    } catch (const std::exception &e) {
        // close() syncs db which throwns an exception as db opened in read-only mode
        cerr << "exception thrown: " << e.what() << endl;
    } catch (...) {

    }

    return 0;
}
