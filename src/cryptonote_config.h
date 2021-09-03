// Copyright (c) 2014-2018, The Monero Project
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
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once

#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>
#include <stdexcept>
#include <chrono>

#define CRYPTONOTE_DNS_TIMEOUT_MS                       20000

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_MAX_BLOCK_SIZE                       500000000  // block header blob limit, never used!
#define CRYPTONOTE_GETBLOCKTEMPLATE_MAX_BLOCK_SIZE      196608 //size of block (bytes) that is the maximum that miners will produce
#define CRYPTONOTE_MAX_TX_SIZE                          1000000000
// #define CRYPTONOTE_MAX_TX_SIZE                          1000000 // Loki's 
#define CRYPTONOTE_MAX_TX_PER_BLOCK                     0x10000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            60
#define CURRENT_TRANSACTION_VERSION                     3
#define CURRENT_BLOCK_MAJOR_VERSION                     1
#define CURRENT_BLOCK_MINOR_VERSION                     0
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              60*60*2
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V9           120*7  // 7xT https://github.com/zawy12/difficulty-algorithms/issues/3, TimeStamp manipulation
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            30
// #define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V2           60*10 // Loki
#define CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE             10
#define CRYPTONOTE_DEFAULT_TX_MIXIN                     9

#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               60
#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V9            11

#define STAKING_REQUIREMENT_LOCK_BLOCKS_EXCESS          20
#define STAKING_PORTIONS                                UINT64_C(0xfffffffffffffffc)
#define MAX_NUMBER_OF_CONTRIBUTORS                      4
#define MIN_PORTIONS                                    (STAKING_PORTIONS / MAX_NUMBER_OF_CONTRIBUTORS)
static_assert(STAKING_PORTIONS % 12 == 0, "Use a multiple of twelve, so that it divides evenly by two, three, or four contributors.");
#define STAKING_AUTHORIZATION_EXPIRATION_WINDOW         (60*60*24*7*2)  // 2 weeks

// MONEY_SUPPLY - total number coins to be generated
#define MONEY_SUPPLY                                    ((uint64_t)(-1))
#define EMISSION_SPEED_FACTOR_PER_MINUTE                (20)
#define FINAL_SUBSIDY_PER_MINUTE                        ((uint64_t)3000000000) // 3 * pow(10, 9)


#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               11

// For local testnet debug purposes allow shrinking the uptime proof frequency
#ifndef UPTIME_PROOF_BASE_MINUTE
#define UPTIME_PROOF_BASE_MINUTE                        60
#endif

#define UPTIME_PROOF_BUFFER_IN_SECONDS                  (5*60) // The acceptable window of time to accept a peer's uptime proof from its reported timestamp
#define UPTIME_PROOF_INITIAL_DELAY_SECONDS              (2*UPTIME_PROOF_BASE_MINUTE) // Delay after startup before sending a proof (to allow connections to be established)
#define UPTIME_PROOF_TIMER_SECONDS                      (5*UPTIME_PROOF_BASE_MINUTE) // How often we check whether we need to send an uptime proof
#define UPTIME_PROOF_FREQUENCY_IN_SECONDS               (60*UPTIME_PROOF_BASE_MINUTE) // How often we resend uptime proofs normally (i.e. after we've seen an uptime proof reply from the network)
#define UPTIME_PROOF_MAX_TIME_IN_SECONDS                (UPTIME_PROOF_FREQUENCY_IN_SECONDS * 2 + UPTIME_PROOF_BUFFER_IN_SECONDS) // How long until proofs of other network service nodes are considered expired

#define STORAGE_SERVER_PING_LIFETIME                    UPTIME_PROOF_FREQUENCY_IN_SECONDS
#define LOKINET_PING_LIFETIME                           UPTIME_PROOF_FREQUENCY_IN_SECONDS

#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2    60000 //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1    20000 // NOTE(loki): For testing suite, //size of block (bytes) after which reward for block calculated using block size - before first fork
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5    300000 //size of block (bytes) after which reward for block calculated using block size - second change, from v5
#define CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE   100000 // size in blocks of the long term block weight median window
#define CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR 50
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600

#define CRYPTONOTE_DISPLAY_DECIMAL_POINT                10
// COIN - number of smallest units in one coin
#define COIN                                            ((uint64_t)10000000000) // pow(10, 10)

#define FEE_PER_KB_OLD                                  ((uint64_t)(COIN)/100) // pow(10, 8)
#define FEE_PER_KB                                      ((uint64_t)2  *  (COIN) / 1000) // 2 * pow(10, 7)
#define FEE_PER_BYTE                                    ((uint64_t)3000)
#define DYNAMIC_FEE_PER_KB_BASE_FEE                     ((uint64_t)2  *  (COIN) / 1000) // 2 * pow(10, 7)
#define DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD            ((uint64_t)10 *  (COIN) ) // 10 * pow(10,10)
#define DYNAMIC_FEE_PER_KB_BASE_FEE_V5                  ((uint64_t)20000000 * (uint64_t)CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 / CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5)
#define DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT        ((uint64_t)3000)


#define RTA_TX_FEE                                      COIN

#define ORPHANED_BLOCKS_MAX_COUNT                       100


#define DIFFICULTY_TARGET_V2                            120  // seconds
#define DIFFICULTY_TARGET_V1                            60  // seconds - before first fork

#define DIFFICULTY_WINDOW                               720 // blocks
#define DIFFICULTY_LAG                                  15  // !!!
#define DIFFICULTY_CUT                                  60  // timestamps to cut after sorting
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG

// Loki's constant
#define DIFFICULTY_WINDOW_V2                            60
#define DIFFICULTY_BLOCKS_COUNT_V2                      (DIFFICULTY_WINDOW_V2 + 1) // added +1 to make N=N

#define DIFFICULTY_WINDOW_V8                            60
#define DIFFICULTY_BLOCKS_COUNT_V8                      DIFFICULTY_WINDOW_V8
#define BLOCKS_EXPECTED_IN_HOURS(val)                   (((60 * 60) / DIFFICULTY_TARGET_V2) * (val))
#define BLOCKS_EXPECTED_IN_DAYS(val)                    (BLOCKS_EXPECTED_IN_HOURS(24) * (val))
#define BLOCKS_EXPECTED_IN_YEARS(val)                   (BLOCKS_EXPECTED_IN_DAYS(365) * (val))

#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V2   DIFFICULTY_TARGET_V2 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS       1

#define DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN             DIFFICULTY_TARGET_V1 //just alias; used by tests

#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000  //by default, blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT_PRE_V4       100    //by default, blocks count in blocks downloading
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT              20     //by default, blocks count in blocks downloading
#define BLOCKS_SYNCHRONIZING_MAX_COUNT                  2048   //must be a power of 2, greater than 128, equal to SEEDHASH_EPOCH_BLOCKS

#define CRYPTONOTE_MEMPOOL_TX_LIVETIME                    (86400*3) //seconds, three days
#define CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME     (86400*7) //seconds, one week

#define MEMPOOL_PRUNE_NON_STANDARD_TX_LIFETIME          (2 * 60 * 60) // seconds, 2 hours


#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT           1000
#define COMMAND_RPC_GET_CHECKPOINTS_MAX_COUNT           256
#define COMMAND_RPC_GET_QUORUM_STATE_MAX_COUNT          256

#define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  1000
#define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   5000

#define P2P_DEFAULT_CONNECTIONS_COUNT_OUT               8
#define P2P_DEFAULT_CONNECTIONS_COUNT_IN                32
#define P2P_DEFAULT_HANDSHAKE_INTERVAL                  60           //secondes
#define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
#define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  250
#define P2P_DEFAULT_CONNECTION_TIMEOUT                  5000       //5 seconds
#define P2P_DEFAULT_SOCKS_CONNECT_TIMEOUT               45         // seconds
#define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             2000       //2 seconds
#define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  //2 minutes
#define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
#define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70
#define P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT            2
#define P2P_DEFAULT_SYNC_SEARCH_CONNECTIONS_COUNT       2
#define P2P_DEFAULT_LIMIT_RATE_UP                       2048       // kB/s
#define P2P_DEFAULT_LIMIT_RATE_DOWN                     8192       // kB/s

#define P2P_FAILED_ADDR_FORGET_SECONDS                  (60*60)     //1 hour
#define P2P_IP_BLOCKTIME                                (60*60*24)  //24 hour
#define P2P_IP_FAILS_BEFORE_BLOCK                       10
#define P2P_IDLE_CONNECTION_KILL_INTERVAL               (5*60) //5 minutes

// TODO(doyle): Deprecate after checkpointing hardfork, remove notion of being
// able to sync non-fluffy blocks, keep here so we can still accept blocks
// pre-hardfork
#define P2P_SUPPORT_FLAG_FLUFFY_BLOCKS                  0x01
#define P2P_SUPPORT_FLAGS                               P2P_SUPPORT_FLAG_FLUFFY_BLOCKS

#define ALLOW_DEBUG_COMMANDS

#define CRYPTONOTE_NAME                         "graft"
#define CRYPTONOTE_POOLDATA_FILENAME            "poolstate.bin"
#define CRYPTONOTE_BLOCKCHAINDATA_FILENAME      "data.mdb"
#define CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME "lock.mdb"
#define P2P_NET_DATA_FILENAME                   "p2pstate.bin"
#define MINER_CONFIG_FILE_NAME                  "miner_conf.json"

#define THREAD_STACK_SIZE                       5 * 1024 * 1024

#define HF_VERSION_MONERO_13                    14
#define HF_VERSION_DYNAMIC_FEE                  4
#define HF_VERSION_MIN_MIXIN_4                  6
#define HF_VERSION_MIN_MIXIN_10                 HF_VERSION_MONERO_13
#define HF_VERSION_ENFORCE_RCT                  6
#define HF_VERSION_PER_BYTE_FEE                 HF_VERSION_MONERO_13

#define HF_VERSION_PER_BYTE_FEE                 cryptonote::network_version_14_bulletproofs
#define HF_VERSION_SMALLER_BP                   cryptonote::network_version_11_infinite_staking
#define HF_VERSION_LONG_TERM_BLOCK_WEIGHT       cryptonote::network_version_11_infinite_staking
#define HF_VERSION_INCREASE_FEE                 cryptonote::network_version_12_checkpointing
#define HF_VERSION_PER_OUTPUT_FEE               cryptonote::network_version_13_enforce_checkpoints
#define HF_VERSION_ED25519_KEY                  cryptonote::network_version_13_enforce_checkpoints
#define HF_VERSION_FEE_BURNING                  cryptonote::network_version_14_blink
#define HF_VERSION_BLINK                        cryptonote::network_version_14_blink

#define PER_KB_FEE_QUANTIZATION_DECIMALS        8

#define HASH_OF_HASHES_STEP                     256

#define DEFAULT_TXPOOL_MAX_WEIGHT               648000000ull // 3 days at 300000, in bytes

#define BULLETPROOF_MAX_OUTPUTS                 16

#define CRYPTONOTE_PRUNING_STRIPE_SIZE          4096 // the smaller, the smoother the increase
#define CRYPTONOTE_PRUNING_LOG_STRIPES          3 // the higher, the more space saved
#define CRYPTONOTE_PRUNING_TIP_BLOCKS           5500 // the smaller, the more space saved
//#define CRYPTONOTE_PRUNING_DEBUG_SPOOF_SEED

// New constants are intended to go here
namespace config
{
  uint64_t const DEFAULT_FEE_ATOMIC_XMR_PER_KB = 500; // Just a placeholder!  Change me!
  uint8_t const FEE_CALCULATION_MAX_RETRIES = 10;
  uint64_t const DEFAULT_DUST_THRESHOLD = ((uint64_t)2000000000); // 2 * pow(10, 9)
  uint64_t const BASE_REWARD_CLAMP_THRESHOLD = ((uint64_t)100000000); // pow(10, 8)
  std::string const P2P_REMOTE_DEBUG_TRUSTED_PUB_KEY = "0000000000000000000000000000000000000000000000000000000000000000";

  uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 90; // 'G'
  uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 91;
  uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 102; // 'J' (https://xmr.llcoins.net/addresstests.html)
  uint16_t const P2P_DEFAULT_PORT = 18980;
  uint16_t const RPC_DEFAULT_PORT = 18981;
  uint16_t const ZMQ_RPC_DEFAULT_PORT = 18982;
  uint16_t const QNET_DEFAULT_PORT = 18983;
  boost::uuids::uuid const NETWORK_ID = { {
        0x54 ,0x68, 0x65, 0x20, 0x41, 0x72 , 0x74, 0x20, 0x6F, 0x66, 0x20, 0x57, 0x61, 0x72, 0x20, 0x35
    } };

  // https://monero.stackexchange.com/questions/2886/how-can-i-create-a-new-monero-genesis-block?rq=1
  std::string const GENESIS_TX = "013c01ff0001ffffffffffff03029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101dbb56baf25ba96a9e186214df0725d5aa898514fcce283013338fd9de0ae8f95";
  uint32_t const GENESIS_NONCE = 10000;

#if 0 // TODO: Remove for Graft
  uint64_t const GOVERNANCE_REWARD_INTERVAL_IN_BLOCKS = ((60 * 60 * 24 * 7) / DIFFICULTY_TARGET_V2);
  std::string const GOVERNANCE_WALLET_ADDRESS[] =
  {
    "LCFxT37LAogDn1jLQKf4y7aAqfi21DjovX9qyijaLYQSdrxY1U5VGcnMJMjWrD9RhjeK5Lym67wZ73uh9AujXLQ1RKmXEyL", // hardfork v7-10
    "LDBEN6Ut4NkMwyaXWZ7kBEAx8X64o6YtDhLXUP26uLHyYT4nFmcaPU2Z2fauqrhTLh4Qfr61pUUZVLaTHqAdycETKM1STrz", // hardfork v11
  };
#endif

  namespace testnet
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 84; // 'F'
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 85;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 112; // 'K' (https://xmr.llcoins.net/addresstests.html)
    uint16_t const P2P_DEFAULT_PORT = 28880;
    uint16_t const RPC_DEFAULT_PORT = 28881;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 28882;
    uint16_t const QNET_DEFAULT_PORT = 38883;
    boost::uuids::uuid const NETWORK_ID = { {
      //0x12 ,0x30, 0xF1, 0x71 , 0x61, 0x04 , 0x41, 0x61, 0x17, 0x31, 0x00, 0x82, 0x16, 0xA1, 0xA1, 0x11
      0x54 ,0x68, 0x65, 0x20, 0x41, 0x72 , 0x74, 0x20, 0x6F, 0x66, 0x20, 0x57, 0x61, 0x72, 0x20, 0x36
    } };
    std::string const GENESIS_TX = "013c01ff0001ffffffffffff03029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101dbb56baf25ba96a9e186214df0725d5aa898514fcce283013338fd9de0ae8f95";
    uint32_t const GENESIS_NONCE = 10001;
#if 0 //TODO: remove fro Graft
    uint64_t const GOVERNANCE_REWARD_INTERVAL_IN_BLOCKS = 1000;
    std::string const GOVERNANCE_WALLET_ADDRESS[] =
    {
      "T6SUprTYE5rQpep9iQFxyPcKVd91DFR1fQ1Qsyqp5eYLiFc8XuYd3reRE71qDL8c3DXioUbDEpDFdaUpetnL37NS1R3rzoKxi", // hardfork v7-9
      "T6TzkJb5EiASaCkcH7idBEi1HSrpSQJE1Zq3aL65ojBMPZvqHNYPTL56i3dncGVNEYCG5QG5zrBmRiVwcg6b1cRM1SRNqbp44", // hardfork v10
    };
#endif  

  }

  namespace stagenet
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 24;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 25;
    // TODO: subaddress prefix
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 36;
    uint16_t const P2P_DEFAULT_PORT = 38056;
    uint16_t const RPC_DEFAULT_PORT = 38057;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 38058;
    uint16_t const QNET_DEFAULT_PORT = 38059;
    boost::uuids::uuid const NETWORK_ID = { {
               0x54 ,0x68, 0x65, 0x20, 0x41, 0x72 , 0x74, 0x20, 0x6F, 0x66, 0x20, 0x57, 0x61, 0x72, 0x20, 0x37
      } };
    std::string const GENESIS_TX = "013c01ff0001ffffffffffff03029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101a9be144ebd3c5bc0e74ebaa61be19231280771c155d31c449979bc50dc854aa3";
    uint32_t const GENESIS_NONCE = 10002;
#if 0 // TODO: Remove for Graft
    uint64_t const GOVERNANCE_REWARD_INTERVAL_IN_BLOCKS = ((60 * 60 * 24 * 7) / DIFFICULTY_TARGET_V2);
    std::string const GOVERNANCE_WALLET_ADDRESS[] =
    {
      "59f7FCwYMiwMnFr8HwsnfJ2hK3DYB1tryhjsfmXqEBJojKyqKeNWoaDaZaauoZPiZHUYp2wJuy5s9H96qy4q9xUVCXXHmTU", // hardfork v7-9
      "59f7FCwYMiwMnFr8HwsnfJ2hK3DYB1tryhjsfmXqEBJojKyqKeNWoaDaZaauoZPiZHUYp2wJuy5s9H96qy4q9xUVCXXHmTU", // hardfork v10
    };
#endif     
  }
}

namespace cryptonote
{
  enum network_version
  {
    network_version_7 = 7,
    network_version_8,
    network_version_9,
    network_version_10_decrease_block_reward,    // decrease block reward
    network_version_11_cn_variant2_pow,          // Monero V8/CN variant 2 PoW
    network_version_12_reverse_waltz_pow,        // Graft CryptoNight Reverse Waltz PoW,
    network_version_13_rta_txs_rta_mining,       // RTA transactions, RTA mining
    network_version_14_bulletproofs,             // bullet-proofs
    network_version_15_disable_non_bullet_proof, // disable non-bullet-proof
    network_version_16_32_days_stake_period,     // stake period up to 32 days
    network_version_17_randomx_pow,              // monero v14 merge
    network_version_18_infinite_staking,         // Loki's service nodes/infinite staking
    network_version_19_checkpointing,            // Loki's checkpointing
    network_version_20_enforce_checkpoints,      // force Loki's checkpointing
    network_version_21_blink,                    // Loki's blink txes 
    network_version_count,
  };
  enum network_type : uint8_t
  {
    MAINNET = 0,
    TESTNET,
    STAGENET,
    FAKECHAIN,
    UNDEFINED = 255
  };
  struct config_t
  {
    uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
    uint64_t CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
    uint64_t CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;
    uint16_t P2P_DEFAULT_PORT;
    uint16_t RPC_DEFAULT_PORT;
    uint16_t ZMQ_RPC_DEFAULT_PORT;
    uint16_t QNET_DEFAULT_PORT;
    boost::uuids::uuid NETWORK_ID;
    std::string GENESIS_TX;
    uint32_t GENESIS_NONCE;
  };
  inline const config_t& get_config(network_type nettype, int hard_fork_version = 7)
  {
    static config_t mainnet = {
      ::config::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::P2P_DEFAULT_PORT,
      ::config::RPC_DEFAULT_PORT,
      ::config::ZMQ_RPC_DEFAULT_PORT,
      ::config::QNET_DEFAULT_PORT,
      ::config::NETWORK_ID,
      ::config::GENESIS_TX
    };

    static config_t testnet = {
      ::config::testnet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::testnet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::testnet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::testnet::P2P_DEFAULT_PORT,
      ::config::testnet::RPC_DEFAULT_PORT,
      ::config::testnet::ZMQ_RPC_DEFAULT_PORT,
      ::config::testnet::QNET_DEFAULT_PORT,
      ::config::testnet::NETWORK_ID,
      ::config::testnet::GENESIS_TX,
      ::config::testnet::GENESIS_NONCE
    };

    static config_t stagenet = {
      ::config::stagenet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::stagenet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::stagenet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::stagenet::P2P_DEFAULT_PORT,
      ::config::stagenet::RPC_DEFAULT_PORT,
      ::config::stagenet::ZMQ_RPC_DEFAULT_PORT,
      ::config::stagenet::QNET_DEFAULT_PORT,
      ::config::stagenet::NETWORK_ID,
      ::config::stagenet::GENESIS_TX,
      ::config::stagenet::GENESIS_NONCE
    };

    switch (nettype)
    {
      case MAINNET:   return mainnet;
      case FAKECHAIN: return mainnet;
      case TESTNET: return testnet;
      case STAGENET: return stagenet;
    }

      default: throw std::runtime_error("Invalid network type");
    }
  };
}

