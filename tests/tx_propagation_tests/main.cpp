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

#include "wallet/wallet2_api.h"

#include "include_base_utils.h"
#include "cryptonote_config.h"
#include "string_coding.h"

#include <boost/chrono/chrono.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/tokenizer.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <vector>
#include <atomic>
#include <functional>
#include <string>
#include <chrono>
#include <thread>

using namespace std;

typedef pair<string, uint64_t> TxRecord;

/*!
 * \brief generate_transactions - generates transactions and stores them to disk
 * \param num                   - number of transactions to generate
 * \param out_txes              - vector of tx ids generated
 * \param output_dir            - directory where transaction files will be written. filename will be <tx_id>.gtx
 * \return                      - true on success
 */
bool generate_transactions(size_t num, vector<string> &out_txes,
                           const std::string &output_dir)
{
    return false;
}

/*!
 * \brief send_transactions - sends pre-generated transactions from input dir to the pool
 * \param input_dir         - input dir with transaction files. (*.gtx)
 * \param output_timestamps - vector of pairs <tx_id, timestamp> - where
 * \return
 */
bool send_transactions(const string &input_dir, vector<TxRecord> &output_timestamps)
{
    return false;
}

/*!
 * \brief monitor_transactions - waits for transactions to be placed into tx pool.
 *                               once tx found in pool, it added to output vector 'result' with the timestamp when it found
 * \param txs_to_wait          - vector of txes to wait for
 * \param timeout_s            - wait no longer than timeout_s
 * \param result               - pairs <tx_id, timestamp> will be written here
 * \return                     - true on success
 */
bool monitor_transactions(const vector<string> &txs_to_wait, size_t timeout_s, vector<TxRecord> &result)
{

}

/*
 CLI interface: "tx-test" <command>
 where  <command> is one of the following:
         "generate" - generates transactions and saves them to directory passed in --output-dir option
         "send"     - sends transactions and saves result (file where each line is <tx_id>:<timestamp>) into file passed as --output-file arg
         "monitor"  - monitoring transaction pool for the transactions passed in file which name is passed as "--input-file" arg

         --wallet-path=<wallet_path>, all commands
         --wallet-password=<wallet_password>, all commands
         --daemon-address=<daemon-address>, only monitor and send commands
         --output-dir=<directory where to write transaction files>, only for "generate" command
         --input-dir=<directory where to read transaction files>,  only for "send" command
         --output-file=<file where report will be written to>, for  "send" and "monitor" commands
         --input-file=<file where monitor will read transaciton ids from>, only for "monitor" command
**/


int main(int argc, char** argv)
{
    epee::string_tools::set_module_name_and_folder(argv[0]);
    mlog_configure("", true);
    mlog_set_log_level(1);

    return 0;
}
