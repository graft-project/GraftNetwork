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
#include "common/int-util.h"
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <string>

#include <iostream>
#include <functional>


using namespace std;
namespace po = boost::program_options;

using namespace cryptonote;

/*
# Tom Harold (Degnr8) WT
# Modified by Zawy to be a weighted-Weighted Harmonic Mean (WWHM)
# No limits in rise or fall rate should be employed.
# MTP should not be used.
k = (N+1)/2  * T

# original algorithm
d=0, t=0, j=0
for i = height - N+1 to height  # (N most recent blocks)
    # TS = timestamp
    solvetime = TS[i] - TS[i-1]
    solvetime = 10*T if solvetime > 10*T
    solvetime = -9*T if solvetime < -9*T
    j++
    t +=  solvetime * j
    d +=D[i] # sum the difficulties
next i
t=T*N/2 if t < T*N/2  # in case of startup weirdness, keep t reasonable
next_D = d * k / t
*/

#define DIFFICULTY_WINDOW_V3                            60
#define DIFFICULTY_BLOCKS_COUNT_V3                      DIFFICULTY_WINDOW_V3

namespace {
#if defined(__x86_64__)
  static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high) {
    low = mul128(a, b, &high);
  }
#endif
}

difficulty_type next_difficulty_masari(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {

  if (timestamps.size() > DIFFICULTY_BLOCKS_COUNT_V3)
  {
    timestamps.resize(DIFFICULTY_BLOCKS_COUNT_V3);
    cumulative_difficulties.resize(DIFFICULTY_BLOCKS_COUNT_V3);
  }

  size_t length = timestamps.size();
  assert(length == cumulative_difficulties.size());
  if (length <= 1) {
    return 1;
  }

  uint64_t weighted_timespans = 0;
  uint64_t target;

  if (true) {
    uint64_t previous_max = timestamps[0];
    for (size_t i = 1; i < length; i++) {
      uint64_t timespan;
      uint64_t max_timestamp;

      if (timestamps[i] > previous_max) {
        max_timestamp = timestamps[i];
      } else {
        max_timestamp = previous_max;
      }

      timespan = max_timestamp - previous_max;
      if (timespan == 0) {
        timespan = 1;
      } else if (timespan > 10 * target_seconds) {
        timespan = 10 * target_seconds;
      }

      weighted_timespans += i * timespan;
      previous_max = max_timestamp;
    }
    // adjust = 0.99 for N=60, leaving the + 1 for now as it's not affecting N
    target = 99 * (((length + 1) / 2) * target_seconds) / 100;
  }

  uint64_t minimum_timespan = target_seconds * length / 2;
  if (weighted_timespans < minimum_timespan) {
    weighted_timespans = minimum_timespan;
  }

  difficulty_type total_work = cumulative_difficulties.back() - cumulative_difficulties.front();
  assert(total_work > 0);

  uint64_t low, high;
  mul(total_work, target, low, high);
  if (high != 0) {
    return 0;
  }
  return low / weighted_timespans;
}



class DAA_Tester
{
public:
    typedef std::function<cryptonote::difficulty_type (std::vector<uint64_t>,
                                                       std::vector<cryptonote::difficulty_type>, size_t target_seconds)> DifficultyFunc;

    cryptonote::difficulty_type next_difficulty(uint64_t timestamp, cryptonote::difficulty_type difficulty, size_t target_seconds)
    {
        size_t begin, end;
        if (m_counter < DIFFICULTY_WINDOW + DIFFICULTY_LAG) {
            begin = 0;
            end = min(m_counter, (size_t) DIFFICULTY_WINDOW);
        } else {
            end = m_counter - DIFFICULTY_LAG;
            begin = end - DIFFICULTY_WINDOW;
        }

        cryptonote::difficulty_type result = m_difficulty_func(
            vector<uint64_t>(m_timestamps.begin() + begin, m_timestamps.begin() + end),
            vector<uint64_t>(m_cumulative_difficulties.begin() + begin,
                             m_cumulative_difficulties.begin() + end), DIFFICULTY_TARGET_V2);

        m_timestamps.push_back(timestamp);
        m_cumulative_difficulties.push_back(m_cumulative_difficulty += difficulty);
        ++m_counter;
        return result;
    }

    void reset()
    {
        m_counter = 0;
        m_cumulative_difficulty = 0;
        m_timestamps.clear();
        m_cumulative_difficulties.clear();
    }

    void setDifficultyFunc(DifficultyFunc df)
    {
        m_difficulty_func = df;
    }

private:
    std::vector<uint64_t> m_timestamps;
    std::vector<cryptonote::difficulty_type> m_cumulative_difficulties;
    DifficultyFunc m_difficulty_func;
    uint64_t m_counter = 0;
    uint64_t m_cumulative_difficulty = 0;
};


int main(int argc, char **argv)
{
    string input_file;
    string output_file;

    int log_level;

    DAA_Tester daa_tester;
    daa_tester.setDifficultyFunc(cryptonote::next_difficulty);

    try {
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")
                ("input-file",      po::value<string>()->required(), "input file")
                ("output-file",     po::value<string>()->required(), "output file")
                ("algorithm",       po::value<string>(), "algorithm")
                ("log-level",       po::value<int>(&log_level)->default_value(1), "log-level");

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

        if (vm.count("algorithm")) {
            string algo_s = vm["algorithm"].as<string>();
            if (algo_s == "masari") {
                daa_tester.setDifficultyFunc(next_difficulty_masari);
            }

        }


        input_file  = vm["input-file"].as<string>();

        LOG_PRINT_L0("5");

        output_file = vm["output-file"].as<string>();
        //output_file = "out2.csv";
        LOG_PRINT_L0("reading from " << input_file);
        ifstream in(input_file);

        if (!in.is_open()) {
            LOG_ERROR("Error opening file " << input_file);
            return false;
        }


        ofstream output(output_file);

        string line;


        while (getline(in, line)) {

            LOG_PRINT_L0("processing line: " << line);

            if (line.find(";") == 0)
                continue;

            typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
            boost::char_separator<char> sep{","};
            vector<string> tokens;
            tokenizer tok{line, sep};
            for (const auto &t : tok) {

                tokens.push_back(boost::trim_copy(t));
            }

            if (tokens.size() != 5) {
                LOG_ERROR("Error parsing row: " << line << ", tokens size: " << tokens.size());
                return -1;
            }

            uint64_t block_num = boost::lexical_cast<uint64_t>(tokens[0]);
            uint64_t timestamp = boost::lexical_cast<uint64_t>(tokens[1]);
            uint64_t difficulty = boost::lexical_cast<uint64_t>(tokens[3]);
            uint64_t next_difficulty = daa_tester.next_difficulty(timestamp, difficulty, DIFFICULTY_TARGET_V2);
            output << block_num << ", " << next_difficulty << std::endl;
        }

    } catch (const std::exception &e) {
        // close() syncs db which throwns an exception as db opened in read-only mode
        cerr << "exception thrown: " << e.what() << endl;
    } catch (...) {

    }

    return 0;
}
