// Copyright (c) 2014-2019, The Monero Project
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
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio.hpp> // cross-platform way to include a header with 'ntohl' 
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/verification_context.h"
#include "cryptonote_basic/difficulty.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "cryptonote_core/service_node_voting.h"
#include "rpc/rpc_handler.h"
#include "common/varint.h"
#include "common/perf_timer.h"
#include "checkpoints/checkpoints.h"

#include "cryptonote_core/service_node_quorum_cop.h"
#include "cryptonote_core/service_node_list.h"
#include "common/loki.h"

namespace
{
  template<typename T>
  std::string compress_integer_array(const std::vector<T> &v)
  {
    std::string s;
    s.resize(v.size() * (sizeof(T) * 8 / 7 + 1));
    char *ptr = (char*)s.data();
    for (const T &t: v)
      tools::write_varint(ptr, t);
    s.resize(ptr - s.data());
    return s;
  }

  template<typename T>
  std::vector<T> decompress_integer_array(const std::string &s)
  {
    std::vector<T> v;
    v.reserve(s.size());
    int read = 0;
    const std::string::const_iterator end = s.end();
    for (std::string::const_iterator i = s.begin(); i != end; std::advance(i, read))
    {
      T t;
      read = tools::read_varint(std::string::const_iterator(i), s.end(), t);
      CHECK_AND_ASSERT_THROW_MES(read > 0 && read <= 256, "Error decompressing data");
      v.push_back(t);
    }
    return v;
  }
}

namespace cryptonote
{
  //-----------------------------------------------
#define CORE_RPC_STATUS_OK   "OK"
#define CORE_RPC_STATUS_BUSY   "BUSY"
#define CORE_RPC_STATUS_NOT_MINING "NOT MINING"
constexpr char const CORE_RPC_STATUS_TX_LONG_POLL_TIMED_OUT[] = "Long polling client timed out before txpool had an update";
constexpr char const CORE_RPC_STATUS_TX_LONG_POLL_MAX_CONNECTIONS[] = "Daemon maxed out long polling connections";

// When making *any* change here, bump minor
// If the change is incompatible, then bump major and set minor to 0
// This ensures CORE_RPC_VERSION always increases, that every change
// has its own version, and that clients can just test major to see
// whether they can talk to a given daemon without having to know in
// advance which version they will stop working with
// Don't go over 32767 for any of these
#define CORE_RPC_VERSION_MAJOR 3
#define CORE_RPC_VERSION_MINOR 4
#define MAKE_CORE_RPC_VERSION(major,minor) (((major)<<16)|(minor))
#define CORE_RPC_VERSION MAKE_CORE_RPC_VERSION(CORE_RPC_VERSION_MAJOR, CORE_RPC_VERSION_MINOR)

  LOKI_RPC_DOC_INTROSPECT
  // Get the node's current height.
  struct COMMAND_RPC_GET_HEIGHT
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint64_t height;            // The current blockchain height according to the queried daemon.
      std::string status;         // Generic RPC error code. "OK" is the success value.
      bool untrusted;             // If the result is obtained using bootstrap mode, and therefore not trusted `true`, or otherwise `false`.
      std::string hash;           // Hash of the block at the current height
      uint64_t immutable_height;  // The latest height in the blockchain that can not be reorganized from (backed by atleast 2 Service Node, or 1 hardcoded checkpoint, 0 if N/A).
      std::string immutable_hash; // Hash of the highest block in the chain that can not be reorganized.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(hash)
        KV_SERIALIZE(immutable_height)
        KV_SERIALIZE(immutable_hash)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get all blocks info. Binary request.
  struct COMMAND_RPC_GET_BLOCKS_FAST
  {

    struct request_t
    {
      std::list<crypto::hash> block_ids; // First 10 blocks id goes sequential, next goes in pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block
      uint64_t    start_height;          // The starting block's height.
      bool        prune;                 // Prunes the blockchain, drops off 7/8 off the block iirc.
      bool        no_miner_tx;           // Optional (false by default).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(block_ids)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(prune)
        KV_SERIALIZE_OPT(no_miner_tx, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct tx_output_indices
    {
      std::vector<uint64_t> indices; // Array of unsigned int.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(indices)
      END_KV_SERIALIZE_MAP()
    };

    struct block_output_indices
    {
      std::vector<tx_output_indices> indices; // Array of TX output indices:

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(indices)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::vector<block_complete_entry> blocks;         // Array of block complete entries
      uint64_t    start_height;                         // The starting block's height.
      uint64_t    current_height;                       // The current block height.
      std::string status;                               // General RPC error code. "OK" means everything looks good.
      std::vector<block_output_indices> output_indices; // Array of indices.
      bool untrusted;                                   // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blocks)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(current_height)
        KV_SERIALIZE(status)
        KV_SERIALIZE(output_indices)
        KV_SERIALIZE(untrusted) 
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get blocks by height. Binary request.
  struct COMMAND_RPC_GET_BLOCKS_BY_HEIGHT
  {
    struct request_t
    {
      std::vector<uint64_t> heights;         // List of block heights

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(heights)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::vector<block_complete_entry> blocks; // Array of block complete entries
      std::string status;                       // General RPC error code. "OK" means everything looks good.
      bool untrusted;                           // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blocks)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };


  LOKI_RPC_DOC_INTROSPECT
  // Get the known blocks hashes which are not on the main chain.
  struct COMMAND_RPC_GET_ALT_BLOCKS_HASHES
  {
    struct request_t
    {
        BEGIN_KV_SERIALIZE_MAP()
        END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
        std::vector<std::string> blks_hashes; // List of alternative blocks hashes to main chain.
        std::string status;                   // General RPC error code. "OK" means everything looks good.
        bool untrusted;                       // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

        BEGIN_KV_SERIALIZE_MAP()
            KV_SERIALIZE(blks_hashes)
            KV_SERIALIZE(status)
            KV_SERIALIZE(untrusted)
        END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get hashes. Binary request.
  struct COMMAND_RPC_GET_HASHES_FAST
  {

    struct request_t
    {
      std::list<crypto::hash> block_ids; // First 10 blocks id goes sequential, next goes in pow(2,n) offset, like 2, 4, 8, 16, 32, 64 and so on, and the last one is always genesis block */
      uint64_t    start_height;          // The starting block's height.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(block_ids)
        KV_SERIALIZE(start_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::vector<crypto::hash> m_block_ids; // Binary array of hashes, See block_ids above.
      uint64_t    start_height;              // The starting block's height.
      uint64_t    current_height;            // The current block height.
      std::string status;                    // General RPC error code. "OK" means everything looks good.
      bool untrusted;                        // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(m_block_ids)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(current_height)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_GET_RANDOM_OUTS
  {
      struct request_t
      {
        std::vector<std::string> amounts;
        uint32_t count;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(amounts)
          KV_SERIALIZE(count)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<request_t> request;
    
      
      struct output {
        std::string public_key;
        uint64_t global_index;
        std::string rct; // 64+64+64 characters long (<rct commit> + <encrypted mask> + <rct amount>)

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(public_key)
          KV_SERIALIZE(global_index)
          KV_SERIALIZE(rct)
        END_KV_SERIALIZE_MAP()
      };

      struct amount_out 
      {
        uint64_t amount;
        std::vector<output> outputs;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(amount)
          KV_SERIALIZE(outputs)
        END_KV_SERIALIZE_MAP()
      };
      
      struct response_t
      {
        std::vector<amount_out> amount_outs;
        std::string Error;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(amount_outs)
          KV_SERIALIZE(Error)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // TODO: Undocumented light wallet RPC call
  struct COMMAND_RPC_SUBMIT_RAW_TX
  {
      struct request_t
      {
        std::string address;
        std::string view_key;
        std::string tx;
        bool blink;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(address)
          KV_SERIALIZE(view_key)
          KV_SERIALIZE(tx)
          KV_SERIALIZE_OPT(blink, false)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<request_t> request;
    
      
      struct response_t
      {
        std::string status;
        std::string error;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(status)
          KV_SERIALIZE(error)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // TODO: Undocumented light wallet RPC call
  struct COMMAND_RPC_LOGIN
  {
      struct request_t
      {
        std::string address;
        std::string view_key;
        bool create_account;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(address)
          KV_SERIALIZE(view_key)
          KV_SERIALIZE(create_account)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<request_t> request;

      struct response_t
      {
        std::string status;
        std::string reason;
        bool new_address;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(status)
          KV_SERIALIZE(reason)
          KV_SERIALIZE(new_address)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // TODO: Undocumented light wallet RPC call
  struct COMMAND_RPC_IMPORT_WALLET_REQUEST
  {
      struct request_t
      {
        std::string address;
        std::string view_key;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(address)
          KV_SERIALIZE(view_key)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<request_t> request;

      struct response_t
      {
        std::string payment_id;
        uint64_t import_fee;
        bool new_request;
        bool request_fulfilled;
        std::string payment_address;
        std::string status;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(payment_id)
          KV_SERIALIZE(import_fee)
          KV_SERIALIZE(new_request)
          KV_SERIALIZE(request_fulfilled)
          KV_SERIALIZE(payment_address)
          KV_SERIALIZE(status)
        END_KV_SERIALIZE_MAP()
      };
      typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Look up one or more transactions by hash.
  struct COMMAND_RPC_GET_TRANSACTIONS
  {
    struct request_t
    {
      std::vector<std::string> txs_hashes; // List of transaction hashes to look up.
      bool decode_as_json;                 // Optional (`false` by default). If set true, the returned transaction information will be decoded rather than binary.
      bool prune;                          // Prunes the blockchain, drops off 7/8 off the block iirc. Optional (`False` by default).
      bool split;                          // Optional (`false` by default).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txs_hashes)
        KV_SERIALIZE(decode_as_json) 
        KV_SERIALIZE_OPT(prune, false)
        KV_SERIALIZE_OPT(split, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct entry
    {
      std::string tx_hash;                  // Transaction hash.
      std::string as_hex;                   // Full transaction information as a hex string.
      std::string pruned_as_hex; 
      std::string prunable_as_hex;
      std::string prunable_hash;
      std::string as_json;                  // List of transaction info.
      bool in_pool;                         // States if the transaction is in pool (`true`) or included in a block (`false`).
      bool double_spend_seen;               // States if the transaction is a double-spend (`true`) or not (`false`).
      uint64_t block_height;                // Block height including the transaction.
      uint64_t block_timestamp;             // Unix time at chich the block has been added to the blockchain.
      std::vector<uint64_t> output_indices; // List of transaction indexes.
      bool relayed;
      bool blink;                           // True if this is an approved, blink transaction (only for in_pool transactions or txes in recent blocks)

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(tx_hash)
        KV_SERIALIZE(as_hex)
        KV_SERIALIZE(pruned_as_hex)
        KV_SERIALIZE(prunable_as_hex)
        KV_SERIALIZE(prunable_hash)
        KV_SERIALIZE(as_json)
        KV_SERIALIZE(in_pool)
        KV_SERIALIZE(double_spend_seen)
        if (!this_ref.in_pool)
        {
          KV_SERIALIZE(block_height)
          KV_SERIALIZE(block_timestamp)
          KV_SERIALIZE(output_indices)
        }
        else
        {
          KV_SERIALIZE(relayed)
        }
        KV_SERIALIZE(blink)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      // older compatibility stuff
      std::vector<std::string> txs_as_hex;  // Full transaction information as a hex string (old compatibility parameter)
      std::vector<std::string> txs_as_json; // Transactions decoded as json (old compat)

      // in both old and new
      std::vector<std::string> missed_tx;   // (Optional - returned if not empty) Transaction hashes that could not be found.

      // new style
      std::vector<entry> txs;               // Array of structure entry as follows:
      std::string status;                   // General RPC error code. "OK" means everything looks good.
      bool untrusted;                       // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txs_as_hex)
        KV_SERIALIZE(txs_as_json)
        KV_SERIALIZE(txs)
        KV_SERIALIZE(missed_tx)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Check if outputs have been spent using the key image associated with the output.
  struct COMMAND_RPC_IS_KEY_IMAGE_SPENT
  {
    enum STATUS 
    {
      UNSPENT = 0,
      SPENT_IN_BLOCKCHAIN = 1,
      SPENT_IN_POOL = 2,
    };

    struct request_t
    {
      std::vector<std::string> key_images; // List of key image hex strings to check.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(key_images)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::vector<int> spent_status; // List of statuses for each image checked. Statuses are follows: 0 = unspent, 1 = spent in blockchain, 2 = spent in transaction pool
      std::string status;            // General RPC error code. "OK" means everything looks good.
      bool untrusted;                // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(spent_status)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };


  LOKI_RPC_DOC_INTROSPECT
  // Get global outputs of transactions. Binary request.
  struct COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES
  {
    struct request_t
    {
      crypto::hash txid; // Binary txid.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_VAL_POD_AS_BLOB(txid)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::vector<uint64_t> o_indexes; // List of output indexes
      std::string status;              // General RPC error code. "OK" means everything looks good.
      bool untrusted;                  // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(o_indexes)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct get_outputs_out
  {
    uint64_t amount; // Amount of Loki in TXID.
    uint64_t index;  

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(amount)
      KV_SERIALIZE(index)
    END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get outputs. Binary request.
  struct COMMAND_RPC_GET_OUTPUTS_BIN
  {
    struct request_t
    {
      std::vector<get_outputs_out> outputs; // Array of structure `get_outputs_out`.
      bool get_txid;                        // TXID

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(outputs)
        KV_SERIALIZE_OPT(get_txid, true)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct outkey
    {
      crypto::public_key key; // The public key of the output.
      rct::key mask;        
      bool unlocked;          // States if output is locked (`false`) or not (`true`).
      uint64_t height;        // Block height of the output.
      crypto::hash txid;      // Transaction id.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_VAL_POD_AS_BLOB(key)
        KV_SERIALIZE_VAL_POD_AS_BLOB(mask)
        KV_SERIALIZE(unlocked)
        KV_SERIALIZE(height)
        KV_SERIALIZE_VAL_POD_AS_BLOB(txid)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::vector<outkey> outs; // List of outkey information.
      std::string status;       // General RPC error code. "OK" means everything looks good.
      bool untrusted;           // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(outs)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_GET_OUTPUTS
  {
    struct request_t
    {
      std::vector<get_outputs_out> outputs; // Array of structure `get_outputs_out`.
      bool get_txid;                        // Request the TXID/hash of the transaction as well.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(outputs)
        KV_SERIALIZE(get_txid)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct outkey
    {
      std::string key;  // The public key of the output.
      std::string mask; 
      bool unlocked;    // States if output is locked (`false`) or not (`true`).
      uint64_t height;  // Block height of the output.
      std::string txid; // Transaction id.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(key)
        KV_SERIALIZE(mask)
        KV_SERIALIZE(unlocked)
        KV_SERIALIZE(height)
        KV_SERIALIZE(txid)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::vector<outkey> outs; // List of outkey information.
      std::string status;       // General RPC error code. "OK" means everything looks good.
      bool untrusted;           // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(outs)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Broadcast a raw transaction to the network.
  struct COMMAND_RPC_SEND_RAW_TX
  {
    struct request_t
    {
      std::string tx_as_hex; // Full transaction information as hexidecimal string.
      bool do_not_relay;     // (Optional: Default false) Stop relaying transaction to other nodes.  Ignored if `blink` is true.
      bool do_sanity_checks; // (Optional: Default true) Verify TX params have sane values.
      bool blink;            // (Optional: Default false) Submit this as a blink tx rather than into the mempool.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(tx_as_hex)
        KV_SERIALIZE_OPT(do_not_relay, false)
        KV_SERIALIZE_OPT(do_sanity_checks, true)
        KV_SERIALIZE_OPT(blink, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.
      std::string reason; // Additional information. Currently empty, "Not relayed" if transaction was accepted but not relayed, or some descriptive message of why the tx failed.
      bool not_relayed;   // Transaction was not relayed (true) or relayed (false).
      bool untrusted;     // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).
      tx_verification_context tvc;
      bool sanity_check_failed;
      blink_result blink_status; // 0 for a non-blink tx.  For a blink tx: 1 means rejected, 2 means accepted, 3 means timeout.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(reason)
        KV_SERIALIZE(not_relayed)
        KV_SERIALIZE(sanity_check_failed)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(tvc)
        KV_SERIALIZE_ENUM(blink_status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Start mining on the daemon.
  struct COMMAND_RPC_START_MINING
  {
    struct request_t
    {
      std::string miner_address;        // Account address to mine to.
      uint64_t    threads_count;        // Number of mining thread to run.
      bool        do_background_mining; // States if the mining should run in background (`true`) or foreground (`false`).
      bool        ignore_battery;       // States if battery state (on laptop) should be ignored (`true`) or not (`false`).
      uint64_t    num_blocks;           // Mine until the blockchain has this many new blocks, then stop (no limit if 0, the default)
      bool        slow_mining;          // Do slow mining (i.e. don't allocate RandomX cache); primarily intended for testing

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(miner_address)
        KV_SERIALIZE(threads_count)
        KV_SERIALIZE(do_background_mining)
        KV_SERIALIZE(ignore_battery)
        KV_SERIALIZE_OPT(num_blocks, uint64_t{0})
        KV_SERIALIZE_OPT(slow_mining, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Retrieve general information about the state of your node and the network.
  struct COMMAND_RPC_GET_INFO
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                   // General RPC error code. "OK" means everything looks good.
      uint64_t height;                      // Current length of longest chain known to daemon.
      uint64_t target_height;               // The height of the next block in the chain.
      uint64_t immutable_height;            // The latest height in the blockchain that can not be reorganized from (backed by atleast 2 Service Node, or 1 hardcoded checkpoint, 0 if N/A).
      uint64_t difficulty;                  // Network difficulty (analogous to the strength of the network).
      uint64_t target;                      // Current target for next proof of work.
      uint64_t tx_count;                    // Total number of non-coinbase transaction in the chain.
      uint64_t tx_pool_size;                // Number of transactions that have been broadcast but not included in a block.
      uint64_t alt_blocks_count;            // Number of alternative blocks to main chain.
      uint64_t outgoing_connections_count;  // Number of peers that you are connected to and getting information from.
      uint64_t incoming_connections_count;  // Number of peers connected to and pulling from your node.
      uint64_t rpc_connections_count;       // Number of RPC client connected to the daemon (Including this RPC request).
      uint64_t white_peerlist_size;         // White Peerlist Size
      uint64_t grey_peerlist_size;          // Grey Peerlist Size
      bool mainnet;                         // States if the node is on the mainnet (`true`) or not (`false`).
      bool testnet;                         // States if the node is on the testnet (`true`) or not (`false`).
      bool stagenet;                        // States if the node is on the stagenet (`true`) or not (`false`).
      std::string nettype;                  // Nettype value used.
      std::string top_block_hash;           // Hash of the highest block in the chain.
      std::string immutable_block_hash;     // Hash of the highest block in the chain that can not be reorganized.
      uint64_t cumulative_difficulty;       // Cumulative difficulty of all blocks in the blockchain.
      uint64_t block_size_limit;            // Maximum allowed block size.
      uint64_t block_weight_limit;          // Maximum allowed block weight.
      uint64_t block_size_median;           // Median block size of latest 100 blocks.
      uint64_t block_weight_median;         // Median block weight of latest 100 blocks.
      uint64_t start_time;                  // Start time of the daemon, as UNIX time.
      uint64_t last_storage_server_ping;    // Last ping time of the storage server (0 if never or not running as a service node)
      uint64_t last_lokinet_ping;           // Last ping time of lokinet (0 if never or not running as a service node)
      uint64_t free_space;                  // Available disk space on the node.
      bool offline;                         // States if the node is offline (`true`) or online (`false`).
      bool untrusted;                       // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).
      std::string bootstrap_daemon_address; // Bootstrap node to give immediate usability to wallets while syncing by proxying RPC to it. (Note: the replies may be untrustworthy).
      uint64_t height_without_bootstrap;    // Current length of the local chain of the daemon.
      bool was_bootstrap_ever_used;         // States if a bootstrap node has ever been used since the daemon started.
      uint64_t database_size;               // Current size of Blockchain data.
      bool update_available;                // States if a update is available ('true') and if one is not available ('false').
      std::string version;                  // Current version of software running.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(height)
        KV_SERIALIZE(target_height)
        KV_SERIALIZE(immutable_height)
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(target)
        KV_SERIALIZE(tx_count)
        KV_SERIALIZE(tx_pool_size)
        KV_SERIALIZE(alt_blocks_count)
        KV_SERIALIZE(outgoing_connections_count)
        KV_SERIALIZE(incoming_connections_count)
        KV_SERIALIZE(rpc_connections_count)
        KV_SERIALIZE(white_peerlist_size)
        KV_SERIALIZE(grey_peerlist_size)
        KV_SERIALIZE(mainnet)
        KV_SERIALIZE(testnet)
        KV_SERIALIZE(stagenet)
        KV_SERIALIZE(nettype)
        KV_SERIALIZE(top_block_hash)
        KV_SERIALIZE(immutable_block_hash)
        KV_SERIALIZE(cumulative_difficulty)
        KV_SERIALIZE(block_size_limit)
        KV_SERIALIZE_OPT(block_weight_limit, (uint64_t)0)
        KV_SERIALIZE(block_size_median)
        KV_SERIALIZE_OPT(block_weight_median, (uint64_t)0)
        KV_SERIALIZE(start_time)
        KV_SERIALIZE(last_storage_server_ping)
        KV_SERIALIZE(last_lokinet_ping)
        KV_SERIALIZE(free_space)
        KV_SERIALIZE(offline)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(bootstrap_daemon_address)
        KV_SERIALIZE(height_without_bootstrap)
        KV_SERIALIZE(was_bootstrap_ever_used)
        KV_SERIALIZE(database_size)
        KV_SERIALIZE(update_available)
        KV_SERIALIZE(version)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //-----------------------------------------------
  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_GET_NET_STATS
  {
    struct request_t
    {

      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::string status;
      uint64_t start_time;
      uint64_t total_packets_in;
      uint64_t total_bytes_in;
      uint64_t total_packets_out;
      uint64_t total_bytes_out;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(start_time)
        KV_SERIALIZE(total_packets_in)
        KV_SERIALIZE(total_bytes_in)
        KV_SERIALIZE(total_packets_out)
        KV_SERIALIZE(total_bytes_out)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Stop mining on the daemon.
  struct COMMAND_RPC_STOP_MINING
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get the mining status of the daemon.
  struct COMMAND_RPC_MINING_STATUS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;


    struct response_t
    {
      std::string status;                // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.
      bool active;                       // States if mining is enabled (`true`) or disabled (`false`).
      uint64_t speed;                    // Mining power in hashes per seconds.
      uint32_t threads_count;            // Number of running mining threads.
      std::string address;               // Account address daemon is mining to. Empty if not mining.
      std::string pow_algorithm;         // Current hashing algorithm name
      bool is_background_mining_enabled; // States if the mining is running in background (`true`) or foreground (`false`).
      uint8_t bg_idle_threshold;         // Background mining, the minimum amount of time in average the CPU should idle in percentage.
      uint8_t bg_min_idle_seconds;       // Background mining, how long the minimum amount of time is for the idle threshold.
      bool bg_ignore_battery;            // Background mining, if true mining does not adjust power depending on battery percentage remaining.
      uint8_t bg_target;                 // Background mining, how much percentage of CPU(?) to consume, default 40%.
      uint32_t block_target;             // The expected time to solve per block, i.e. DIFFICULTY_TARGET_V2
      uint64_t block_reward;             // Block reward for the current block being mined.
      uint64_t difficulty;               // The difficulty for the current block being mined.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(active)
        KV_SERIALIZE(speed)
        KV_SERIALIZE(threads_count)
        KV_SERIALIZE(address)
        KV_SERIALIZE(pow_algorithm)
        KV_SERIALIZE(is_background_mining_enabled)
        KV_SERIALIZE(bg_idle_threshold)
        KV_SERIALIZE(bg_min_idle_seconds)
        KV_SERIALIZE(bg_ignore_battery)
        KV_SERIALIZE(bg_target)
        KV_SERIALIZE(block_target)
        KV_SERIALIZE(block_reward)
        KV_SERIALIZE(difficulty)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Save the blockchain. The blockchain does not need saving and is always saved when modified, 
  // however it does a sync to flush the filesystem cache onto the disk for safety purposes against Operating System or Hardware crashes.
  struct COMMAND_RPC_SAVE_BC
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Look up how many blocks are in the longest chain known to the node.
  struct COMMAND_RPC_GETBLOCKCOUNT
  {
    typedef std::list<std::string> request;

    struct response_t
    {
      uint64_t count;     // Number of blocks in longest chain seen by the node.
      std::string status; // General RPC error code. "OK" means everything looks good.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(count)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Look up a block's hash by its height.
  struct COMMAND_RPC_GETBLOCKHASH
  {
    typedef std::vector<uint64_t> request; // Block height (int array of length 1).
    typedef std::string response;          // Block hash (string).
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get a block template on which mining a new block.
  struct COMMAND_RPC_GETBLOCKTEMPLATE
  {
    struct request_t
    {
      uint64_t reserve_size;      // Max 255 bytes
      std::string wallet_address; // Address of wallet to receive coinbase transactions if block is successfully mined.
      std::string prev_block;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(reserve_size)
        KV_SERIALIZE(wallet_address)
        KV_SERIALIZE(prev_block)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint64_t difficulty;         // Difficulty of next block.
      uint64_t height;             // Height on which to mine.
      uint64_t reserved_offset;    // Reserved offset.
      uint64_t expected_reward;    // Coinbase reward expected to be received if block is successfully mined.
      std::string prev_hash;       // Hash of the most recent block on which to mine the next block.
      std::string seed_hash;       // RandomX current seed hash
      std::string next_seed_hash;  // RandomX upcoming seed hash
      blobdata blocktemplate_blob; // Blob on which to try to mine a new block.
      blobdata blockhashing_blob;  // Blob on which to try to find a valid nonce.
      std::string status;          // General RPC error code. "OK" means everything looks good.
      bool untrusted;              // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(height)
        KV_SERIALIZE(reserved_offset)
        KV_SERIALIZE(expected_reward)
        KV_SERIALIZE(prev_hash)
        KV_SERIALIZE(blocktemplate_blob)
        KV_SERIALIZE(blockhashing_blob)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(seed_hash)
        KV_SERIALIZE(next_seed_hash)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Submit a mined block to the network.
  struct COMMAND_RPC_SUBMITBLOCK
  {
    typedef std::vector<std::string> request; // Block blob data - array of strings; list of block blobs which have been mined. See get_block_template to get a blob on which to mine.
    struct response_t
    {
      std::string status; // Block submit status.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Developer only.
  struct COMMAND_RPC_GENERATEBLOCKS
  {
    struct request_t
    {
      uint64_t amount_of_blocks; 
      std::string wallet_address;
      std::string prev_block;
      uint32_t starting_nonce;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount_of_blocks)
        KV_SERIALIZE(wallet_address)
        KV_SERIALIZE(prev_block)
        KV_SERIALIZE_OPT(starting_nonce, (uint32_t)0)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint64_t height;
      std::vector<std::string> blocks;
      std::string status; // General RPC error code. "OK" means everything looks good.
      
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE(blocks)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct block_header_response
  {
      uint8_t major_version;                  // The major version of the loki protocol at this block height.
      uint8_t minor_version;                  // The minor version of the loki protocol at this block height.
      uint64_t timestamp;                     // The unix time at which the block was recorded into the blockchain.
      std::string prev_hash;                  // The hash of the block immediately preceding this block in the chain.
      uint32_t nonce;                         // A cryptographic random one-time number used in mining a Loki block.
      bool orphan_status;                     // Usually `false`. If `true`, this block is not part of the longest chain.
      uint64_t height;                        // The number of blocks preceding this block on the blockchain.
      uint64_t depth;                         // The number of blocks succeeding this block on the blockchain. A larger number means an older block.
      std::string hash;                       // The hash of this block.
      difficulty_type difficulty;             // The strength of the Loki network based on mining power.
      difficulty_type cumulative_difficulty;  // The cumulative strength of the Loki network based on mining power.
      uint64_t reward;                        // The amount of new generated in this block and rewarded to the miner, foundation and service Nodes. Note: 1 LOKI = 1e12 atomic units.
      uint64_t miner_reward;                  // The amount of new generated in this block and rewarded to the miner. Note: 1 LOKI = 1e12 atomic units. 
      uint64_t block_size;                    // The block size in bytes.
      uint64_t block_weight;                  // The block weight in bytes.
      uint64_t num_txes;                      // Number of transactions in the block, not counting the coinbase tx.
      std::string pow_hash;                   // The hash of the block's proof of work.
      uint64_t long_term_weight;              // Long term weight of the block.
      std::string miner_tx_hash;              // The TX hash of the miner transaction
      std::string service_node_winner;        // Service node that received a reward for this block

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(major_version)
        KV_SERIALIZE(minor_version)
        KV_SERIALIZE(timestamp)
        KV_SERIALIZE(prev_hash)
        KV_SERIALIZE(nonce)
        KV_SERIALIZE(orphan_status)
        KV_SERIALIZE(height)
        KV_SERIALIZE(depth)
        KV_SERIALIZE(hash)
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(cumulative_difficulty)
        KV_SERIALIZE(reward)
        KV_SERIALIZE(miner_reward)
        KV_SERIALIZE(block_size)
        KV_SERIALIZE_OPT(block_weight, (uint64_t)0)
        KV_SERIALIZE(num_txes)
        KV_SERIALIZE(pow_hash)
        KV_SERIALIZE_OPT(long_term_weight, (uint64_t)0)
        KV_SERIALIZE(miner_tx_hash)
        KV_SERIALIZE(service_node_winner)
      END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  // Block header information for the most recent block is easily retrieved with this method. No inputs are needed.
  struct COMMAND_RPC_GET_LAST_BLOCK_HEADER
  {
    struct request_t
    {
      bool fill_pow_hash; // Tell the daemon if it should fill out pow_hash field.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                 // General RPC error code. "OK" means everything looks good.
      block_header_response block_header; // A structure containing block header information.
      bool untrusted;                     // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Block header information can be retrieved using either a block's hash or height. This method includes a block's hash as an input parameter to retrieve basic information about the block.
  struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH
  {
    struct request_t
    {
      std::string hash;   // The block's SHA256 hash.
      bool fill_pow_hash; // Tell the daemon if it should fill out pow_hash field.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(hash)
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                 // General RPC error code. "OK" means everything looks good.
      block_header_response block_header; // A structure containing block header information.
      bool untrusted;                     // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Similar to get_block_header_by_hash above, this method includes a block's height as an input parameter to retrieve basic information about the block.
  struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT
  {
    struct request_t
    {
      uint64_t height;    // The block's height.
      bool fill_pow_hash; // Tell the daemon if it should fill out pow_hash field.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                 // General RPC error code. "OK" means everything looks good.
      block_header_response block_header; // A structure containing block header information.
      bool untrusted;                     // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Full block information can be retrieved by either block height or hash, like with the above block header calls. 
  // For full block information, both lookups use the same method, but with different input parameters.
  struct COMMAND_RPC_GET_BLOCK
  {
    struct request_t
    {
      std::string hash;   // The block's hash.
      uint64_t height;    // The block's height.
      bool fill_pow_hash; // Tell the daemon if it should fill out pow_hash field.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(hash)
        KV_SERIALIZE(height)
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                 // General RPC error code. "OK" means everything looks good.
      block_header_response block_header; // A structure containing block header information. See get_last_block_header.
      std::string miner_tx_hash;          // Miner transaction information
      std::vector<std::string> tx_hashes; // List of hashes of non-coinbase transactions in the block. If there are no other transactions, this will be an empty list.
      std::string blob;                   // Hexadecimal blob of block information.
      std::string json;                   // JSON formatted block details.
      bool untrusted;                     // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_header)
        KV_SERIALIZE(miner_tx_hash)
        KV_SERIALIZE(tx_hashes)
        KV_SERIALIZE(status)
        KV_SERIALIZE(blob)
        KV_SERIALIZE(json)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct peer 
  {
    uint64_t id;           // Peer id.
    std::string host;      // IP address in string format.
    uint32_t ip;           // IP address in integer format.
    uint16_t port;         // TCP port the peer is using to connect to loki network.
    uint16_t rpc_port;     // RPC port the peer is using
    uint64_t last_seen;    // Unix time at which the peer has been seen for the last time
    uint32_t pruning_seed; //

    peer() = default;

    peer(uint64_t id, const std::string &host, uint64_t last_seen, uint32_t pruning_seed, uint16_t rpc_port)
      : id(id), host(host), ip(0), port(0), rpc_port(rpc_port), last_seen(last_seen), pruning_seed(pruning_seed)
    {}
    peer(uint64_t id, const std::string &host, uint16_t port, uint64_t last_seen, uint32_t pruning_seed, uint16_t rpc_port)
      : id(id), host(host), ip(0), port(port), rpc_port(rpc_port), last_seen(last_seen), pruning_seed(pruning_seed)
    {}
    peer(uint64_t id, uint32_t ip, uint16_t port, uint64_t last_seen, uint32_t pruning_seed, uint16_t rpc_port)
      : id(id), host(std::to_string(ip)), ip(ip), port(port), rpc_port(rpc_port), last_seen(last_seen), pruning_seed(pruning_seed)
    {}

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(id)
      KV_SERIALIZE(host)
      KV_SERIALIZE(ip)
      KV_SERIALIZE(port)
      KV_SERIALIZE_OPT(rpc_port, (uint16_t)0)
      KV_SERIALIZE(last_seen)
      KV_SERIALIZE_OPT(pruning_seed, (uint32_t)0)
    END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get the known peers list.
  struct COMMAND_RPC_GET_PEER_LIST
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;           // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.
      std::vector<peer> white_list; // Array of online peer structure.
      std::vector<peer> gray_list;  // Array of offline peer structure.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(white_list)
        KV_SERIALIZE(gray_list)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Set the log hash rate display mode.
  struct COMMAND_RPC_SET_LOG_HASH_RATE
  {
    struct request_t
    {
      bool visible; // States if hash rate logs should be visible (true) or hidden (false)

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(visible)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Set the daemon log level. By default, log level is set to `0`.
  struct COMMAND_RPC_SET_LOG_LEVEL
  {
    struct request_t
    {
      int8_t level; // Daemon log level to set from `0` (less verbose) to `4` (most verbose)

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(level)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Set the daemon log categories. Categories are represented as a comma separated list of `<Category>:<level>` (similarly to syslog standard `<Facility>:<Severity-level>`), where:
  // Category is one of the following: * (all facilities), default, net, net.http, net.p2p, logging, net.trottle, blockchain.db, blockchain.db.lmdb, bcutil, checkpoints, net.dns, net.dl,
  // i18n, perf,stacktrace, updates, account, cn ,difficulty, hardfork, miner, blockchain, txpool, cn.block_queue, net.cn, daemon, debugtools.deserialize, debugtools.objectsizes, device.ledger, 
  // wallet.gen_multisig, multisig, bulletproofs, ringct, daemon.rpc, wallet.simplewallet, WalletAPI, wallet.ringdb, wallet.wallet2, wallet.rpc, tests.core.
  //
  // Level is one of the following: FATAL - higher level, ERROR, WARNING, INFO, DEBUG, TRACE.
  // Lower level A level automatically includes higher level. By default, categories are set to:
  // `*:WARNING,net:FATAL,net.p2p:FATAL,net.cn:FATAL,global:INFO,verify:FATAL,stacktrace:INFO,logging:INFO,msgwriter:INFO`
  // Setting the categories to "" prevent any logs to be outputed.
  struct COMMAND_RPC_SET_LOG_CATEGORIES
  {
    struct request_t
    {
      std::string categories; // Optional, daemon log categories to enable

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(categories)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;     // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.
      std::string categories; // Daemon log enabled categories

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(categories)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct tx_info
  {
    std::string id_hash;                // The transaction ID hash.
    std::string tx_json;                // JSON structure of all information in the transaction
    uint64_t blob_size;                 // The size of the full transaction blob.
    uint64_t weight;                    // The weight of the transaction.
    uint64_t fee;                       // The amount of the mining fee included in the transaction, in atomic units.
    std::string max_used_block_id_hash; // Tells the hash of the most recent block with an output used in this transaction.
    uint64_t max_used_block_height;     // Tells the height of the most recent block with an output used in this transaction.
    bool kept_by_block;                 // States if the tx was included in a block at least once (`true`) or not (`false`).
    uint64_t last_failed_height;        // If the transaction validation has previously failed, this tells at what height that occured.
    std::string last_failed_id_hash;    // Like the previous, this tells the previous transaction ID hash.
    uint64_t receive_time;              // The Unix time that the transaction was first seen on the network by the node.
    bool relayed;                       // States if this transaction has been relayed
    uint64_t last_relayed_time;         // Last unix time at which the transaction has been relayed.
    bool do_not_relay;                  // States if this transaction should not be relayed.
    bool double_spend_seen;             // States if this transaction has been seen as double spend.
    std::string tx_blob;                // Hexadecimal blob represnting the transaction.
    bool blink;                         // True if this is a signed blink transaction

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(id_hash)
      KV_SERIALIZE(tx_json)
      KV_SERIALIZE(blob_size)
      KV_SERIALIZE_OPT(weight, (uint64_t)0)
      KV_SERIALIZE(fee)
      KV_SERIALIZE(max_used_block_id_hash)
      KV_SERIALIZE(max_used_block_height)
      KV_SERIALIZE(kept_by_block)
      KV_SERIALIZE(last_failed_height)
      KV_SERIALIZE(last_failed_id_hash)
      KV_SERIALIZE(receive_time)
      KV_SERIALIZE(relayed)
      KV_SERIALIZE(last_relayed_time)
      KV_SERIALIZE(do_not_relay)
      KV_SERIALIZE(double_spend_seen)
      KV_SERIALIZE(tx_blob)
    END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  struct spent_key_image_info
  {
    std::string id_hash;                 // Key image.
    std::vector<std::string> txs_hashes; // List of tx hashes of the txes (usually one) spending that key image.

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(id_hash)
      KV_SERIALIZE(txs_hashes)
    END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  // Show information about valid transactions seen by the node but not yet mined into a block, 
  // as well as spent key image information for the txpool in the node's memory.
  struct COMMAND_RPC_GET_TRANSACTION_POOL
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                                 // General RPC error code. "OK" means everything looks good.
      std::vector<tx_info> transactions;                  // List of transactions in the mempool are not in a block on the main chain at the moment:
      std::vector<spent_key_image_info> spent_key_images; // List of spent output key images:
      bool untrusted;                                     // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(transactions)
        KV_SERIALIZE(spent_key_images)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get hashes from transaction pool. Binary request.
  struct COMMAND_RPC_GET_TRANSACTION_POOL_HASHES_BIN
  {
    struct request_t
    {
      bool         long_poll;        // Optional: If true, this call is blocking until timeout OR tx pool has changed since the last query. TX pool change is detected by comparing the hash of all the hashes in the tx pool.
      crypto::hash tx_pool_checksum; // Optional: If `long_poll` is true the caller must pass the hashes of all their known tx pool hashes, XOR'ed together.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(long_poll, false)
        KV_SERIALIZE_VAL_POD_AS_BLOB_OPT(tx_pool_checksum, crypto::hash{})
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                  // General RPC error code. "OK" means everything looks good.
      std::vector<crypto::hash> tx_hashes; // List of transaction hashes,
      bool untrusted;                      // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(tx_hashes)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get hashes from transaction pool.
  struct COMMAND_RPC_GET_TRANSACTION_POOL_HASHES
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                 // General RPC error code. "OK" means everything looks good.
      std::vector<std::string> tx_hashes; // List of transaction hashes,
      bool untrusted;                     // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(tx_hashes)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct tx_backlog_entry
  {
    uint64_t weight;       // 
    uint64_t fee;          // Fee in Loki measured in atomic units.
    uint64_t time_in_pool;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get all transaction pool backlog.
  struct COMMAND_RPC_GET_TRANSACTION_POOL_BACKLOG
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                    // General RPC error code. "OK" means everything looks good.
      std::vector<tx_backlog_entry> backlog; // Array of structures tx_backlog_entry (in binary form):
      bool untrusted;                        // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE_CONTAINER_POD_AS_BLOB(backlog)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct txpool_histo
  {
    uint32_t txs;   // Number of transactions.
    uint64_t bytes; // Size in bytes.

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(txs)
      KV_SERIALIZE(bytes)
    END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  struct txpool_stats
  {
    uint64_t bytes_total;            // Total size of all transactions in pool.
    uint32_t bytes_min;              // Min transaction size in pool.
    uint32_t bytes_max;              // Max transaction size in pool.
    uint32_t bytes_med;              // Median transaction size in pool.
    uint64_t fee_total;              // Total fee's in pool in atomic units.
    uint64_t oldest;                 // Unix time of the oldest transaction in the pool.
    uint32_t txs_total;              // Total number of transactions.
    uint32_t num_failing;            // Bumber of failing transactions.
    uint32_t num_10m;                // Number of transactions in pool for more than 10 minutes.
    uint32_t num_not_relayed;        // Number of non-relayed transactions.
    uint64_t histo_98pc;             // the time 98% of txes are "younger" than.
    std::vector<txpool_histo> histo; // List of txpool histo.
    uint32_t num_double_spends;      // Number of double spend transactions.

    txpool_stats(): bytes_total(0), bytes_min(0), bytes_max(0), bytes_med(0), fee_total(0), oldest(0), txs_total(0), num_failing(0), num_10m(0), num_not_relayed(0), histo_98pc(0), num_double_spends(0) {}

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(bytes_total)
      KV_SERIALIZE(bytes_min)
      KV_SERIALIZE(bytes_max)
      KV_SERIALIZE(bytes_med)
      KV_SERIALIZE(fee_total)
      KV_SERIALIZE(oldest)
      KV_SERIALIZE(txs_total)
      KV_SERIALIZE(num_failing)
      KV_SERIALIZE(num_10m)
      KV_SERIALIZE(num_not_relayed)
      KV_SERIALIZE(histo_98pc)
      KV_SERIALIZE_CONTAINER_POD_AS_BLOB(histo)
      KV_SERIALIZE(num_double_spends)
    END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get the transaction pool statistics.
  struct COMMAND_RPC_GET_TRANSACTION_POOL_STATS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;      // General RPC error code. "OK" means everything looks good.
      txpool_stats pool_stats; // List of pool stats:
      bool untrusted;          // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(pool_stats)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Retrieve information about incoming and outgoing connections to your node.
  struct COMMAND_RPC_GET_CONNECTIONS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.
      std::list<connection_info> connections; // List of all connections and their info:

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(connections)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Similar to get_block_header_by_height above, but for a range of blocks. 
  // This method includes a starting block height and an ending block height as 
  // parameters to retrieve basic information about the range of blocks.
  struct COMMAND_RPC_GET_BLOCK_HEADERS_RANGE
  {
    struct request_t
    {
      uint64_t start_height; // The starting block's height.
      uint64_t end_height;   // The ending block's height.
      bool fill_pow_hash;    // Tell the daemon if it should fill out pow_hash field.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(end_height)
        KV_SERIALIZE_OPT(fill_pow_hash, false);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                         // General RPC error code. "OK" means everything looks good.
      std::vector<block_header_response> headers; // Array of block_header (a structure containing block header information. See get_last_block_header).
      bool untrusted;                             // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(headers)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Send a command to the daemon to safely disconnect and shut down.
  struct COMMAND_RPC_STOP_DAEMON
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get daemon bandwidth limits.
  struct COMMAND_RPC_GET_LIMIT
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };

    typedef epee::misc_utils::struct_init<request_t> request;
    struct response_t
    {
      std::string status;  // General RPC error code. "OK" means everything looks good.
      uint64_t limit_up;   // Upload limit in kBytes per second.
      uint64_t limit_down; // Download limit in kBytes per second.
      bool untrusted;      // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(limit_up)
        KV_SERIALIZE(limit_down)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Set daemon bandwidth limits.
  struct COMMAND_RPC_SET_LIMIT
  {
    struct request_t
    {
      int64_t limit_down;  // Download limit in kBytes per second (-1 reset to default, 0 don't change the current limit)
      int64_t limit_up;    // Upload limit in kBytes per second (-1 reset to default, 0 don't change the current limit)

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(limit_down)
        KV_SERIALIZE(limit_up)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
    
    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.
      int64_t limit_up;   // Upload limit in kBytes per second.
      int64_t limit_down; // Download limit in kBytes per second.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(limit_up)
        KV_SERIALIZE(limit_down)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Limit number of Outgoing peers.
  struct COMMAND_RPC_OUT_PEERS
  {
    struct request_t
    {
	  uint64_t out_peers; // Max number of outgoing peers
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(out_peers)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;
    
    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Limit number of Incoming peers.
  struct COMMAND_RPC_IN_PEERS
  {
    struct request_t
    {
      uint64_t in_peers;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(in_peers)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Look up information regarding hard fork voting and readiness.
  struct COMMAND_RPC_HARD_FORK_INFO
  {
    struct request_t
    {
      uint8_t version; // The major block version for the fork.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint8_t version;          // The major block version for the fork.
      bool enabled;             // Tells if hard fork is enforced.
      uint32_t window;          // Number of blocks over which current votes are cast. Default is 10080 blocks.
      uint32_t votes;           // Number of votes towards hard fork.
      uint32_t threshold;       // Minimum percent of votes to trigger hard fork. Default is 80.
      uint8_t voting;           // Hard fork voting status.
      uint32_t state;           // Current hard fork state: 0 (There is likely a hard fork), 1 (An update is needed to fork properly), or 2 (Everything looks good).
      uint64_t earliest_height; // Block height at which hard fork would be enabled if voted in.
      std::string status;       // General RPC error code. "OK" means everything looks good.
      bool untrusted;           // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version)
        KV_SERIALIZE(enabled)
        KV_SERIALIZE(window)
        KV_SERIALIZE(votes)
        KV_SERIALIZE(threshold)
        KV_SERIALIZE(voting)
        KV_SERIALIZE(state)
        KV_SERIALIZE(earliest_height)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get list of banned IPs.
  struct COMMAND_RPC_GETBANS
  {
    struct ban
    {
      std::string host; // Banned host (IP in A.B.C.D form).
      uint32_t ip;      // Banned IP address, in Int format.
      uint32_t seconds; // Local Unix time that IP is banned until.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(host)
        KV_SERIALIZE(ip)
        KV_SERIALIZE(seconds)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;    // General RPC error code. "OK" means everything looks good.
      std::vector<ban> bans; // List of banned nodes:

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(bans)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Ban another node by IP.
  struct COMMAND_RPC_SETBANS
  {
    struct ban
    {
      std::string host; // Host to ban (IP in A.B.C.D form - will support I2P address in the future).
      uint32_t ip;      // IP address to ban, in Int format.
      bool ban;         // Set true to ban.
      uint32_t seconds; // Number of seconds to ban node.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(host)
        KV_SERIALIZE(ip)
        KV_SERIALIZE(ban)
        KV_SERIALIZE(seconds)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      std::vector<ban> bans; // List of nodes to ban.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(bans)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Determine whether a given IP address is banned
  struct COMMAND_RPC_BANNED
  {
    struct request_t
    {
      std::string address; // The IP address to check

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(address)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.
      bool banned;        // True if the given address is banned, false otherwise.
      uint32_t seconds;   // The number of seconds remaining in the ban.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(banned)
        KV_SERIALIZE(seconds)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Flush tx ids from transaction pool..
  struct COMMAND_RPC_FLUSH_TRANSACTION_POOL
  {
    struct request_t
    {
      std::vector<std::string> txids; // Optional, list of transactions IDs to flush from pool (all tx ids flushed if empty).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txids)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get a histogram of output amounts. For all amounts (possibly filtered by parameters), 
  // gives the number of outputs on the chain for that amount. RingCT outputs counts as 0 amount.
  struct COMMAND_RPC_GET_OUTPUT_HISTOGRAM
  {
    struct request_t
    {
      std::vector<uint64_t> amounts; // list of amounts in Atomic Units.
      uint64_t min_count;            // The minimum amounts you are requesting.
      uint64_t max_count;            // The maximum amounts you are requesting.
      bool unlocked;                 // Look for locked only.
      uint64_t recent_cutoff;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amounts);
        KV_SERIALIZE(min_count);
        KV_SERIALIZE(max_count);
        KV_SERIALIZE(unlocked);
        KV_SERIALIZE(recent_cutoff);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct entry
    {
      uint64_t amount;            // Output amount in atomic units.
      uint64_t total_instances;
      uint64_t unlocked_instances;
      uint64_t recent_instances;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount);
        KV_SERIALIZE(total_instances);
        KV_SERIALIZE(unlocked_instances);
        KV_SERIALIZE(recent_instances);
      END_KV_SERIALIZE_MAP()

      entry(uint64_t amount, uint64_t total_instances, uint64_t unlocked_instances, uint64_t recent_instances):
          amount(amount), total_instances(total_instances), unlocked_instances(unlocked_instances), recent_instances(recent_instances) {}
      entry() {}
    };

    struct response_t
    {
      std::string status;           // General RPC error code. "OK" means everything looks good.
      std::vector<entry> histogram; // List of histogram entries:
      bool untrusted;               // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(histogram)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get node current version.
  struct COMMAND_RPC_GET_VERSION
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.
      uint32_t version;   // Node current version.
      bool untrusted;     // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(version)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get the coinbase amount and the fees amount for n last blocks starting at particular height.
  struct COMMAND_RPC_GET_COINBASE_TX_SUM
  {
    struct request_t
    {
      uint64_t height; // Block height from which getting the amounts.
      uint64_t count;  // Number of blocks to include in the sum.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height);
        KV_SERIALIZE(count);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;       // General RPC error code. "OK" means everything looks good.
      uint64_t emission_amount; // Amount of coinbase reward in atomic units.
      uint64_t fee_amount;      // Amount of fees in atomic units.
      uint64_t burn_amount;      // Amount of burnt loki.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(emission_amount)
        KV_SERIALIZE(fee_amount)
        KV_SERIALIZE(burn_amount)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Gives an estimation of per-output + per-byte fees
  struct COMMAND_RPC_GET_BASE_FEE_ESTIMATE
  {
    struct request_t
    {
      uint64_t grace_blocks; // Optional

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(grace_blocks)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;         // General RPC error code. "OK" means everything looks good.
      uint64_t fee_per_byte;      // Amount of fees estimated per byte in atomic units
      uint64_t fee_per_output;    // Amount of fees per output generated by the tx (adds to the `fee_per_byte` per-byte value)
      uint64_t quantization_mask;
      bool untrusted;             // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(fee_per_byte)
        KV_SERIALIZE(fee_per_output)
        KV_SERIALIZE_OPT(quantization_mask, (uint64_t)1)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Display alternative chains seen by the node.
  struct COMMAND_RPC_GET_ALTERNATE_CHAINS
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct chain_info
    {
      std::string block_hash;                // The block hash of the first diverging block of this alternative chain.
      uint64_t height;                       // The block height of the first diverging block of this alternative chain.
      uint64_t length;                       // The length in blocks of this alternative chain, after divergence.
      uint64_t difficulty;                   // The cumulative difficulty of all blocks in the alternative chain.
      std::vector<std::string> block_hashes; 
      std::string main_chain_parent_block;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_hash)
        KV_SERIALIZE(height)
        KV_SERIALIZE(length)
        KV_SERIALIZE(difficulty)
        KV_SERIALIZE(block_hashes)
        KV_SERIALIZE(main_chain_parent_block)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::string status;           // General RPC error code. "OK" means everything looks good.
      std::list<chain_info> chains; // Array of Chains.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(chains)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Update daemon.
  struct COMMAND_RPC_UPDATE
  {
    struct request_t
    {
      std::string command; // Command to use, either check or download.
      std::string path;    // Optional, path where to download the update.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(command);
        KV_SERIALIZE(path);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;   // General RPC error code. "OK" means everything looks good.
      bool update;          // States if an update is available to download (`true`) or not (`false`).
      std::string version;  // Version available for download.
      std::string user_uri;
      std::string auto_uri;
      std::string hash;
      std::string path;     // Path to download the update.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(update)
        KV_SERIALIZE(version)
        KV_SERIALIZE(user_uri)
        KV_SERIALIZE(auto_uri)
        KV_SERIALIZE(hash)
        KV_SERIALIZE(path)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Relay a list of transaction IDs.
  struct COMMAND_RPC_RELAY_TX
  {
    struct request_t
    {
      std::vector<std::string> txids; // Optional, list of transactions IDs to flush from pool (all tx ids flushed if empty).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(txids)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get synchronisation information.
  struct COMMAND_RPC_SYNC_INFO
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct peer
    {
      connection_info info; // Structure of connection info, as defined in get_connections.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(info)
      END_KV_SERIALIZE_MAP()
    };

    struct span
    {
      uint64_t start_block_height; // Block height of the first block in that span.
      uint64_t nblocks;            // Number of blocks in that span.
      std::string connection_id;   // Id of connection.
      uint32_t rate;               // Connection rate.
      uint32_t speed;              // Connection speed.
      uint64_t size;               // Total number of bytes in that span's blocks (including txes).
      std::string remote_address;  // Peer address the node is downloading (or has downloaded) than span from.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(start_block_height)
        KV_SERIALIZE(nblocks)
        KV_SERIALIZE(connection_id)
        KV_SERIALIZE(rate)
        KV_SERIALIZE(speed)
        KV_SERIALIZE(size)
        KV_SERIALIZE(remote_address)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::string status;                // General RPC error code. "OK" means everything looks good. Any other value means that something went wrong.
      uint64_t height;                   // Block height.
      uint64_t target_height;            // Target height the node is syncing from (optional, absent if node is fully synced).
      uint32_t next_needed_pruning_seed;
      std::list<peer> peers;             // Array of Peer structure
      std::list<span> spans;             // Array of Span Structure.
      std::string overview;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(height)
        KV_SERIALIZE(target_height)
        KV_SERIALIZE(next_needed_pruning_seed)
        KV_SERIALIZE(peers)
        KV_SERIALIZE(spans)
        KV_SERIALIZE(overview)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  //---------- Graft RTA commands ---------------------
  struct COMMAND_RPC_SUPERNODE_GET_STAKES
  {
    struct request
    {
      std::string supernode_public_id;
      std::string network_address;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(supernode_public_id)
        KV_SERIALIZE(network_address)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      int64_t status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_SUPERNODE_STAKES
  {
    struct supernode_stake
    {
      uint64_t amount;
      unsigned int tier;
      uint64_t block_height;
      uint64_t unlock_time;
      std::string supernode_public_id;
      std::string supernode_public_address;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount)
        KV_SERIALIZE(tier)
        KV_SERIALIZE(block_height)
        KV_SERIALIZE(unlock_time)
        KV_SERIALIZE(supernode_public_id)
        KV_SERIALIZE(supernode_public_address)
      END_KV_SERIALIZE_MAP()
    };

    struct request
    {
      uint64_t block_height;
      std::vector<supernode_stake> stakes;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_height)
        KV_SERIALIZE(stakes)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      int64_t status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_SUPERNODE_GET_BLOCKCHAIN_BASED_LIST
  {
    struct request
    {
      std::string supernode_public_id;
      std::string network_address;
      uint64_t    last_received_block_height;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(supernode_public_id)
        KV_SERIALIZE(network_address)
        KV_SERIALIZE(last_received_block_height)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      int64_t status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_SUPERNODE_BLOCKCHAIN_BASED_LIST
  {
    struct supernode
    {
      std::string supernode_public_id;
      std::string supernode_public_address;
      uint64_t    amount;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(supernode_public_id)
        KV_SERIALIZE(supernode_public_address)
        KV_SERIALIZE(amount)
      END_KV_SERIALIZE_MAP()
    };

    struct tier
    {
      std::vector<supernode> supernodes;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(supernodes)
      END_KV_SERIALIZE_MAP()
    };

    struct request
    {
      uint64_t block_height;
      std::vector<tier> tiers;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(block_height)
        KV_SERIALIZE(tiers)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      int64_t status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_SUPERNODE_ANNOUNCE
  {
    struct request
    {

      std::string supernode_public_id;
      uint64_t height;
      std::string signature;
      std::string network_address;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(supernode_public_id)
        KV_SERIALIZE(height)
        KV_SERIALIZE(signature)
        KV_SERIALIZE(network_address)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      int64_t status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_BROADCAST
  {
    struct request
    {
      std::string sender_address;
      std::string callback_uri;
      std::string data;
      bool wait_answer;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(sender_address)
        KV_SERIALIZE(callback_uri)
        KV_SERIALIZE(data)
        KV_SERIALIZE(wait_answer)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      int64_t status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_MULTICAST
  {
    struct request
    {
      std::list<std::string> receiver_addresses;
      std::string sender_address;
      std::string callback_uri;
      std::string data;
      bool wait_answer;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(receiver_addresses)
        KV_SERIALIZE(sender_address)
        KV_SERIALIZE(callback_uri)
        KV_SERIALIZE(data)
        KV_SERIALIZE(wait_answer)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      int64_t status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_UNICAST
  {
    struct request
    {
      std::string receiver_address;
      std::string sender_address;
      std::string callback_uri;
      std::string data;
      bool wait_answer;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(receiver_address)
        KV_SERIALIZE(sender_address)
        KV_SERIALIZE(callback_uri)
        KV_SERIALIZE(data)
        KV_SERIALIZE(wait_answer)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      int64_t status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct peer_data
  {
      std::string host;
      uint16_t port;
      uint64_t id;
      int64_t last_seen;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(host)
        KV_SERIALIZE(port)
        KV_SERIALIZE(id)
        KV_SERIALIZE(last_seen)
      END_KV_SERIALIZE_MAP()
  };

  struct route_data
  {
    std::string address;
    uint64_t last_announce_height;
    uint64_t last_announce_time;
    uint64_t max_hop;
    std::vector<peer_data> peers;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(address)
      KV_SERIALIZE(last_announce_height)
      KV_SERIALIZE(last_announce_time)
      KV_SERIALIZE(max_hop)
      KV_SERIALIZE(peers)
    END_KV_SERIALIZE_MAP()
  };

  struct COMMAND_RPC_TUNNEL_DATA
  {
    struct request
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string supernode_address;
      std::vector<route_data> tunnels;
      std::vector<std::string> supernodes_addresses;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(supernode_address)
        KV_SERIALIZE(tunnels)
        KV_SERIALIZE(supernodes_addresses)
      END_KV_SERIALIZE_MAP()
    };
  };

  struct COMMAND_RPC_RTA_STATS
  {
    struct request
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      uint64_t announce_bytes_in;
      uint64_t announce_bytes_out;
      uint64_t broadcast_bytes_in;
      uint64_t broadcast_bytes_out;
      uint64_t multicast_bytes_in;
      uint64_t multicast_bytes_out;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(announce_bytes_in)
        KV_SERIALIZE(announce_bytes_out)
        KV_SERIALIZE(broadcast_bytes_in)
        KV_SERIALIZE(broadcast_bytes_out)
        KV_SERIALIZE(multicast_bytes_in)
        KV_SERIALIZE(multicast_bytes_out)
      END_KV_SERIALIZE_MAP()
    };
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_GET_OUTPUT_DISTRIBUTION
  {
    struct request_t
    {
      std::vector<uint64_t> amounts; // Amounts to look for in atomic units.
      uint64_t from_height;          // (optional, default is 0) starting height to check from.
      uint64_t to_height;            // (optional, default is 0) ending height to check up to.
      bool cumulative;               // (optional, default is false) States if the result should be cumulative (true) or not (false).
      bool binary; 
      bool compress;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amounts)
        KV_SERIALIZE_OPT(from_height, (uint64_t)0)
        KV_SERIALIZE_OPT(to_height, (uint64_t)0)
        KV_SERIALIZE_OPT(cumulative, false)
        KV_SERIALIZE_OPT(binary, true)
        KV_SERIALIZE_OPT(compress, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct distribution
    {
      rpc::output_distribution_data data;
      uint64_t amount; 
      std::string compressed_data;
      bool binary;
      bool compress;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount)
        KV_SERIALIZE_N(data.start_height, "start_height")
        KV_SERIALIZE(binary)
        KV_SERIALIZE(compress)
        if (this_ref.binary)
        {
          if (is_store)
          {
            if (this_ref.compress)
            {
              const_cast<std::string&>(this_ref.compressed_data) = compress_integer_array(this_ref.data.distribution);
              KV_SERIALIZE(compressed_data)
            }
            else
              KV_SERIALIZE_CONTAINER_POD_AS_BLOB_N(data.distribution, "distribution")
          }
          else
          {
            if (this_ref.compress)
            {
              KV_SERIALIZE(compressed_data)
              const_cast<std::vector<uint64_t>&>(this_ref.data.distribution) = decompress_integer_array<uint64_t>(this_ref.compressed_data);
            }
            else
              KV_SERIALIZE_CONTAINER_POD_AS_BLOB_N(data.distribution, "distribution")
          }
        }
        else
          KV_SERIALIZE_N(data.distribution, "distribution")
        KV_SERIALIZE_N(data.base, "base")
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::string status;                      // General RPC error code. "OK" means everything looks good.
      std::vector<distribution> distributions; // 
      bool untrusted;                          // States if the result is obtained using the bootstrap mode, and is therefore not trusted (`true`), or when the daemon is fully synced (`false`).

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(distributions)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_POP_BLOCKS
  {
    struct request_t
    {
      uint64_t nblocks; // Number of blocks in that span.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(nblocks);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status; // General RPC error code. "OK" means everything looks good.
      uint64_t height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_PRUNE_BLOCKCHAIN
  {
    struct request_t
    {
      bool check;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(check, false)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      bool pruned;
      uint32_t pruning_seed;
      std::string status;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(pruned)
        KV_SERIALIZE(pruning_seed)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };


  LOKI_RPC_DOC_INTROSPECT
  // Get the quorum state which is the list of public keys of the nodes who are voting, and the list of public keys of the nodes who are being tested.
  struct COMMAND_RPC_GET_QUORUM_STATE
  {
    static constexpr uint64_t HEIGHT_SENTINEL_VALUE = UINT64_MAX;
    static constexpr uint8_t ALL_QUORUMS_SENTINEL_VALUE = 255;
    struct request_t
    {
      uint64_t start_height; // (Optional): Start height, omit both start and end height to request the latest quorum
      uint64_t end_height;   // (Optional): End height, omit both start and end height to request the latest quorum
      uint8_t  quorum_type;  // (Optional): Set value to request a specific quorum, 0 = Obligation, 1 = Checkpointing, 255 = all quorums, default is all quorums;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(start_height, HEIGHT_SENTINEL_VALUE)
        KV_SERIALIZE_OPT(end_height, HEIGHT_SENTINEL_VALUE)
        KV_SERIALIZE_OPT(quorum_type, ALL_QUORUMS_SENTINEL_VALUE)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct quorum_t
    {
      std::vector<std::string> validators; // Public key of the service node
      std::vector<std::string> workers; // Public key of the service node

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(validators)
        KV_SERIALIZE(workers)
      END_KV_SERIALIZE_MAP()

      BEGIN_SERIALIZE() // NOTE: For store_t_to_json
        FIELD(validators)
        FIELD(workers)
      END_SERIALIZE()
    };

    struct quorum_for_height
    {
      uint64_t height;          // The height the quorums are relevant for
      uint8_t  quorum_type;     // The quorum type
      quorum_t quorum;          // Quorum of Service Nodes
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
        KV_SERIALIZE(quorum_type)
        KV_SERIALIZE(quorum)
      END_KV_SERIALIZE_MAP()

      BEGIN_SERIALIZE() // NOTE: For store_t_to_json
        FIELD(height)
        FIELD(quorum_type)
        FIELD(quorum)
      END_SERIALIZE()
    };

    struct response_t
    {
      std::string status;                     // Generic RPC error code. "OK" is the success value.
      std::vector<quorum_for_height> quorums; // An array of quorums associated with the requested height
      bool untrusted;                         // If the result is obtained using bootstrap mode, and therefore not trusted `true`, or otherwise `false`.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(quorums)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD_RAW
  {
    struct request_t
    {
      std::vector<std::string> args; // (Developer) The arguments used in raw registration, i.e. portions
      bool make_friendly;            // Provide information about how to use the command in the result.
      uint64_t staking_requirement;  // The staking requirement to become a Service Node the registration command will be generated upon

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(args)
        KV_SERIALIZE(make_friendly)
        KV_SERIALIZE(staking_requirement)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;           // Generic RPC error code. "OK" is the success value.
      std::string registration_cmd; // The command to execute in the wallet CLI to register the queried daemon as a Service Node.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(registration_cmd)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_CMD
  {
    struct contribution_t
    {
      std::string address; // The wallet address for the contributor
      uint64_t amount;     // The amount that the contributor will reserve in Loki atomic units towards the staking requirement

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(address)
        KV_SERIALIZE(amount)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      std::string operator_cut;                  // The percentage of cut per reward the operator receives expressed as a string, i.e. "1.1%"
      std::vector<contribution_t> contributions; // Array of contributors for this Service Node
      uint64_t staking_requirement;              // The staking requirement to become a Service Node the registration command will be generated upon

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(operator_cut)
        KV_SERIALIZE(contributions)
        KV_SERIALIZE(staking_requirement)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;           // Generic RPC error code. "OK" is the success value.
      std::string registration_cmd; // The command to execute in the wallet CLI to register the queried daemon as a Service Node.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(registration_cmd)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get the service node public keys of the queried daemon, encoded in hex.
  // The daemon must be started in --service-node mode otherwise this RPC command will fail.
  struct COMMAND_RPC_GET_SERVICE_NODE_KEY
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string service_node_pubkey;         // The queried daemon's service node public key.
      std::string service_node_ed25519_pubkey; // The daemon's service node ed25519 auxiliary public key.
      std::string service_node_x25519_pubkey;  // The daemon's service node x25519 auxiliary public key.
      std::string status;                      // Generic RPC error code. "OK" is the success value.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_pubkey)
        KV_SERIALIZE(service_node_ed25519_pubkey)
        KV_SERIALIZE(service_node_x25519_pubkey)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get the service node private keys of the queried daemon, encoded in hex.  Do not ever share
  // these keys: they would allow someone to impersonate your service node.
  // The daemon must be started in --service-node mode otherwise this RPC command will fail.
  struct COMMAND_RPC_GET_SERVICE_NODE_PRIVKEY
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string service_node_privkey;         // The queried daemon's service node private key.
      std::string service_node_ed25519_privkey; // The daemon's service node ed25519 private key (note that this is in sodium's format, which consists of the private and public keys concatenated together)
      std::string service_node_x25519_privkey;  // The daemon's service node x25519 private key.
      std::string status;                       // Generic RPC error code. "OK" is the success value.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_privkey)
        KV_SERIALIZE(service_node_ed25519_privkey)
        KV_SERIALIZE(service_node_x25519_privkey)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // TODO: Undocumented, -- unused
  struct COMMAND_RPC_PERFORM_BLOCKCHAIN_TEST
  {
    struct request
    {
      uint64_t max_height;
      uint64_t seed;

      BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(max_height)
      KV_SERIALIZE(seed)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status;
      uint64_t res_height;

      BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(status)
      KV_SERIALIZE(res_height)
      END_KV_SERIALIZE_MAP()
    };
  };

  LOKI_RPC_DOC_INTROSPECT
  struct service_node_contribution
  {
    std::string key_image;         // The contribution's key image that is locked on the network.
    std::string key_image_pub_key; // The contribution's key image, public key component
    uint64_t    amount;            // The amount that is locked in this contribution.

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(key_image)
      KV_SERIALIZE(key_image_pub_key)
      KV_SERIALIZE(amount)
    END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  struct service_node_contributor
  {
    uint64_t amount;                                             // The total amount of locked Loki in atomic units for this contributor.
    uint64_t reserved;                                           // The amount of Loki in atomic units reserved by this contributor for this Service Node.
    std::string address;                                         // The wallet address for this contributor rewards are sent to and contributions came from.
    std::vector<service_node_contribution> locked_contributions; // Array of contributions from this contributor.

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(amount)
      KV_SERIALIZE(reserved)
      KV_SERIALIZE(address)
      KV_SERIALIZE(locked_contributions)
    END_KV_SERIALIZE_MAP()
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get information on Service Nodes.
  struct COMMAND_RPC_GET_SERVICE_NODES
  {
    struct request_t
    {
      std::vector<std::string> service_node_pubkeys; // Array of public keys of active Service Nodes to get information about. Pass the empty array to query all Service Nodes.
      bool include_json;                             // When set, the response's as_json member is filled out.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_pubkeys);
        KV_SERIALIZE(include_json);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      struct entry
      {
        std::string                           service_node_pubkey;           // The public key of the Service Node.
        uint64_t                              registration_height;           // The height at which the registration for the Service Node arrived on the blockchain.
        uint16_t                              registration_hf_version;       // The hard fork at which the registration for the Service Node arrived on the blockchain.
        uint64_t                              requested_unlock_height;       // The height at which contributions will be released and the Service Node expires. 0 if not requested yet.
        uint64_t                              last_reward_block_height;      // The last height at which this Service Node received a reward.
        uint32_t                              last_reward_transaction_index; // When multiple Service Nodes register on the same height, the order the transaction arrive dictate the order you receive rewards.
        bool                                  active;                        // True if fully funded and not currently decommissioned (and so `active && !funded` implicitly defines decommissioned)
        bool                                  funded;                        // True if the required stakes have been submitted to activate this Service Node
        uint64_t                              state_height;                  // If active: the state at which registration was completed; if decommissioned: the decommissioning height; if awaiting: the last contribution (or registration) height
        uint32_t                              decommission_count;            // The number of times the Service Node has been decommissioned since registration
        int64_t                               earned_downtime_blocks;        // The number of blocks earned towards decommissioning, or the number of blocks remaining until deregistration if currently decommissioned
        std::array<uint16_t, 3>               service_node_version;          // The major, minor, patch version of the Service Node respectively.
        std::vector<service_node_contributor> contributors;                  // Array of contributors, contributing to this Service Node.
        uint64_t                              total_contributed;             // The total amount of Loki in atomic units contributed to this Service Node.
        uint64_t                              total_reserved;                // The total amount of Loki in atomic units reserved in this Service Node.
        uint64_t                              staking_requirement;           // The staking requirement in atomic units that is required to be contributed to become a Service Node.
        uint64_t                              portions_for_operator;         // The operator percentage cut to take from each reward expressed in portions, see cryptonote_config.h's STAKING_PORTIONS.
        uint64_t                              swarm_id;                      // The identifier of the Service Node's current swarm.
        std::string                           operator_address;              // The wallet address of the operator to which the operator cut of the staking reward is sent to.
        std::string                           public_ip;                     // The public ip address of the service node
        uint16_t                              storage_port;                  // The port number associated with the storage server
        uint16_t                              storage_lmq_port;              // The port number associated with the storage server (lokimq interface)
        uint16_t                              quorumnet_port;                // The port for direct SN-to-SN communication
        std::string                           pubkey_ed25519;                // The service node's ed25519 public key for auxiliary services
        std::string                           pubkey_x25519;                 // The service node's x25519 public key for auxiliary services


        // Service Node Testing
        uint64_t                                           last_uptime_proof;                   // The last time this Service Node's uptime proof was relayed by at least 1 Service Node other than itself in unix epoch time.
        bool                                               storage_server_reachable;            // Whether the node's storage server has been reported as unreachable for a long time
        uint64_t                                           storage_server_reachable_timestamp;  // The last time this Service Node's storage server was contacted
        uint16_t                                           version_major;                       // Major version the node is currently running
        uint16_t                                           version_minor;                       // Minor version the node is currently running
        uint16_t                                           version_patch;                       // Patch version the node is currently running
        std::vector<service_nodes::checkpoint_vote_record> votes;                               // Of the last N checkpoints the Service Node is in a checkpointing quorum, record whether or not the Service Node voted to checkpoint a block

        BEGIN_KV_SERIALIZE_MAP()
            KV_SERIALIZE(service_node_pubkey)
            KV_SERIALIZE(registration_height)
            KV_SERIALIZE(registration_hf_version)
            KV_SERIALIZE(requested_unlock_height)
            KV_SERIALIZE(last_reward_block_height)
            KV_SERIALIZE(last_reward_transaction_index)
            KV_SERIALIZE(active)
            KV_SERIALIZE(funded)
            KV_SERIALIZE(state_height)
            KV_SERIALIZE(decommission_count)
            KV_SERIALIZE(earned_downtime_blocks)
            KV_SERIALIZE(service_node_version)
            KV_SERIALIZE(contributors)
            KV_SERIALIZE(total_contributed)
            KV_SERIALIZE(total_reserved)
            KV_SERIALIZE(staking_requirement)
            KV_SERIALIZE(portions_for_operator)
            KV_SERIALIZE(swarm_id)
            KV_SERIALIZE(operator_address)
            KV_SERIALIZE(public_ip)
            KV_SERIALIZE(storage_port)
            KV_SERIALIZE(storage_lmq_port)
            KV_SERIALIZE(quorumnet_port)
            KV_SERIALIZE(pubkey_ed25519)
            KV_SERIALIZE(pubkey_x25519)

            KV_SERIALIZE(last_uptime_proof)
            KV_SERIALIZE(storage_server_reachable)
            KV_SERIALIZE(storage_server_reachable_timestamp)
            KV_SERIALIZE(version_major)
            KV_SERIALIZE(version_minor)
            KV_SERIALIZE(version_patch)
            KV_SERIALIZE(votes)
        END_KV_SERIALIZE_MAP()
      };

      std::vector<entry> service_node_states; // Array of service node registration information
      uint64_t    height;                     // Current block's height.
      std::string block_hash;                 // Current block's hash.
      std::string status;                     // Generic RPC error code. "OK" is the success value.
      std::string as_json;                    // If `include_json` is set in the request, this contains the json representation of the `entry` data structure


      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_states)
        KV_SERIALIZE(height)
        KV_SERIALIZE(block_hash)
        KV_SERIALIZE(status)
        KV_SERIALIZE(as_json)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get information on the queried daemon's Service Node state.
  struct COMMAND_RPC_GET_SERVICE_NODE_STATUS
  {
    struct request_t
    {
      bool include_json;                             // When set, the response's as_json member is filled out.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(include_json);
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {

      cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response_t::entry service_node_state; // Service node registration information
      uint64_t    height;                     // Current block's height.
      std::string block_hash;                 // Current block's hash.
      std::string status;                     // Generic RPC error code. "OK" is the success value.
      std::string as_json;                    // If `include_json` is set in the request, this contains the json representation of the `entry` data structure


      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(service_node_state)
        KV_SERIALIZE(height)
        KV_SERIALIZE(block_hash)
        KV_SERIALIZE(status)
        KV_SERIALIZE(as_json)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  #define KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(var) \
  if (this_ref.requested_fields.var || !this_ref.requested_fields.explicitly_set) KV_SERIALIZE(var)

  LOKI_RPC_DOC_INTROSPECT
  // Get information on a all (or optionally a random subset) of Service Nodes.
  struct COMMAND_RPC_GET_N_SERVICE_NODES
  {

    // Boolean values indicate whether corresponding
    // fields should be included in the response
    struct requested_fields_t {

      bool explicitly_set = false;          // internal use only: incicates whether one of the other parameters has been explicitly set

      bool service_node_pubkey;
      bool registration_height;
      bool registration_hf_version;
      bool requested_unlock_height;
      bool last_reward_block_height;
      bool last_reward_transaction_index;
      bool active;
      bool funded;
      bool state_height;
      bool decommission_count;
      bool earned_downtime_blocks;

      bool service_node_version;
      bool contributors;
      bool total_contributed;
      bool total_reserved;
      bool staking_requirement;
      bool portions_for_operator;
      bool swarm_id;
      bool operator_address;
      bool public_ip;
      bool storage_port;
      bool storage_lmq_port;
      bool quorumnet_port;
      bool pubkey_ed25519;
      bool pubkey_x25519;

      bool last_uptime_proof;
      bool storage_server_reachable;
      bool storage_server_reachable_timestamp;
      bool version_major;
      bool version_minor;
      bool version_patch;
      bool votes;

      bool block_hash;
      bool height;
      bool target_height;
      bool hardfork;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT2(service_node_pubkey, false)
        KV_SERIALIZE_OPT2(registration_height, false)
        KV_SERIALIZE_OPT2(registration_hf_version, false)
        KV_SERIALIZE_OPT2(requested_unlock_height, false)
        KV_SERIALIZE_OPT2(last_reward_block_height, false)
        KV_SERIALIZE_OPT2(last_reward_transaction_index, false)
        KV_SERIALIZE_OPT2(active, false)
        KV_SERIALIZE_OPT2(funded, false)
        KV_SERIALIZE_OPT2(state_height, false)
        KV_SERIALIZE_OPT2(decommission_count, false)
        KV_SERIALIZE_OPT2(earned_downtime_blocks, false)
        KV_SERIALIZE_OPT2(service_node_version, false)
        KV_SERIALIZE_OPT2(contributors, false)
        KV_SERIALIZE_OPT2(total_contributed, false)
        KV_SERIALIZE_OPT2(total_reserved, false)
        KV_SERIALIZE_OPT2(staking_requirement, false)
        KV_SERIALIZE_OPT2(portions_for_operator, false)
        KV_SERIALIZE_OPT2(swarm_id, false)
        KV_SERIALIZE_OPT2(operator_address, false)
        KV_SERIALIZE_OPT2(public_ip, false)
        KV_SERIALIZE_OPT2(storage_port, false)
        KV_SERIALIZE_OPT2(storage_lmq_port, false)
        KV_SERIALIZE_OPT2(quorumnet_port, false)
        KV_SERIALIZE_OPT2(pubkey_ed25519, false)
        KV_SERIALIZE_OPT2(pubkey_x25519, false)
        KV_SERIALIZE_OPT2(block_hash, false)
        KV_SERIALIZE_OPT2(height, false)
        KV_SERIALIZE_OPT2(target_height, false)
        KV_SERIALIZE_OPT2(hardfork, false)

        KV_SERIALIZE_OPT2(last_uptime_proof, false)
        KV_SERIALIZE_OPT2(storage_server_reachable, false)
        KV_SERIALIZE_OPT2(storage_server_reachable_timestamp, false)
        KV_SERIALIZE_OPT2(version_major, false)
        KV_SERIALIZE_OPT2(version_minor, false)
        KV_SERIALIZE_OPT2(version_patch, false)
        KV_SERIALIZE_OPT2(votes, false)
      END_KV_SERIALIZE_MAP()
    };

    struct request_t
    {
      uint32_t limit; // If non-zero, select a random sample (in random order) of the given number of service nodes to return from the full list.
      bool active_only; // If true, only include results for active (fully staked, not decommissioned) service nodes.
      requested_fields_t fields;

      std::string poll_block_hash; // If specified this changes the behaviour to only return service node records if the block hash is *not* equal to the given hash; otherwise it omits the records and instead sets `"unchanged": true` in the response. This is primarily used to poll for new results where the requested results only change with new blocks.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(limit)
        KV_SERIALIZE(active_only)
        KV_SERIALIZE(fields)
        KV_SERIALIZE(poll_block_hash)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {

      struct entry {
        const requested_fields_t& requested_fields;

        entry(const requested_fields_t& res)
          : requested_fields(res)
        {}

        std::string                           service_node_pubkey;           // The public key of the Service Node.
        uint64_t                              registration_height;           // The height at which the registration for the Service Node arrived on the blockchain.
        uint16_t                              registration_hf_version;       // The hard fork at which the registration for the Service Node arrived on the blockchain.
        uint64_t                              requested_unlock_height;       // The height at which contributions will be released and the Service Node expires. 0 if not requested yet.
        uint64_t                              last_reward_block_height;      // The last height at which this Service Node received a reward.
        uint32_t                              last_reward_transaction_index; // When multiple Service Nodes register on the same height, the order the transaction arrive dictate the order you receive rewards.
        bool                                  active;                        // True if fully funded and not currently decommissioned (and so `active && !funded` implicitly defines decommissioned)
        bool                                  funded;                        // True if the required stakes have been submitted to activate this Service Node
        uint64_t                              state_height;                  // If active: the state at which registration was completed; if decommissioned: the decommissioning height; if awaiting: the last contribution (or registration) height
        uint32_t                              decommission_count;            // The number of times the Service Node has been decommissioned since registration
        int64_t                               earned_downtime_blocks;        // The number of blocks earned towards decommissioning, or the number of blocks remaining until deregistration if currently decommissioned
        std::array<uint16_t, 3>               service_node_version;          // The major, minor, patch version of the Service Node respectively.
        std::vector<service_node_contributor> contributors;                  // Array of contributors, contributing to this Service Node.
        uint64_t                              total_contributed;             // The total amount of Loki in atomic units contributed to this Service Node.
        uint64_t                              total_reserved;                // The total amount of Loki in atomic units reserved in this Service Node.
        uint64_t                              staking_requirement;           // The staking requirement in atomic units that is required to be contributed to become a Service Node.
        uint64_t                              portions_for_operator;         // The operator percentage cut to take from each reward expressed in portions, see cryptonote_config.h's STAKING_PORTIONS.
        uint64_t                              swarm_id;                      // The identifier of the Service Node's current swarm.
        std::string                           operator_address;              // The wallet address of the operator to which the operator cut of the staking reward is sent to.
        std::string                           public_ip;                     // The public ip address of the service node
        uint16_t                              storage_port;                  // The port number associated with the storage server
        uint16_t                              storage_lmq_port;              // The port number associated with the storage server (lokimq interface)
        uint16_t                              quorumnet_port;                // The port for direct SN-to-SN communication
        std::string                           pubkey_ed25519;                // The service node's ed25519 public key for auxiliary services
        std::string                           pubkey_x25519;                 // The service node's x25519 public key for auxiliary services

        // Service Node Testing
        uint64_t                                           last_uptime_proof;                   // The last time this Service Node's uptime proof was relayed by at least 1 Service Node other than itself in unix epoch time.
        bool                                               storage_server_reachable;            // Whether the node's storage server has been reported as unreachable for a long time
        uint64_t                                           storage_server_reachable_timestamp;  // The last time this Service Node's storage server was contacted
        uint16_t                                           version_major;                       // Major version the node is currently running
        uint16_t                                           version_minor;                       // Minor version the node is currently running
        uint16_t                                           version_patch;                       // Patch version the node is currently running
        std::vector<service_nodes::checkpoint_vote_record> votes;                               // Of the last N checkpoints the Service Node is in a checkpointing quorum, record whether or not the Service Node voted to checkpoint a block

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(service_node_pubkey);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(registration_height);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(registration_hf_version);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(requested_unlock_height);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(last_reward_block_height);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(last_reward_transaction_index);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(active);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(funded);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(state_height);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(decommission_count);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(earned_downtime_blocks);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(service_node_version);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(contributors);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(total_contributed);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(total_reserved);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(staking_requirement);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(portions_for_operator);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(swarm_id);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(operator_address);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(public_ip);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(storage_port);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(storage_lmq_port);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(quorumnet_port);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(pubkey_ed25519);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(pubkey_x25519);

          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(last_uptime_proof);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(storage_server_reachable);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(storage_server_reachable_timestamp);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(version_major);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(version_minor);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(version_patch);
          KV_SERIALIZE_ENTRY_FIELD_IF_REQUESTED(votes);
        END_KV_SERIALIZE_MAP()
      };

      requested_fields_t fields; // @NoLokiRPCDocGen Internal use only, not serialized
      bool polling_mode;         // @NoLokiRPCDocGen Internal use only, not serialized

      std::vector<entry> service_node_states; // Array of service node registration information
      uint64_t    height;                     // Current block's height.
      uint64_t    target_height;              // Blockchain's target height.
      std::string block_hash;                 // Current block's hash.
      bool        unchanged;                  // Will be true (and `service_node_states` omitted) if you gave the current block hash to poll_block_hash
      uint8_t     hardfork;                   // Current hardfork version.
      std::string status;                     // Generic RPC error code. "OK" is the success value.

      BEGIN_KV_SERIALIZE_MAP()
        if (!this_ref.unchanged) {
          KV_SERIALIZE(service_node_states)
        }
        KV_SERIALIZE(status)
        if (this_ref.fields.height) {
          KV_SERIALIZE(height)
        }
        if (this_ref.fields.target_height) {
          KV_SERIALIZE(target_height)
        }
        if (this_ref.fields.block_hash || (this_ref.polling_mode && !this_ref.unchanged)) {
          KV_SERIALIZE(block_hash)
        }
        if (this_ref.fields.hardfork) {
          KV_SERIALIZE(hardfork)
        }
        if (this_ref.polling_mode) {
          KV_SERIALIZE(unchanged);
        }
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_STORAGE_SERVER_PING
  {
    struct request
    {
      int version_major; // Storage Server Major version
      int version_minor; // Storage Server Minor version
      int version_patch; // Storage Server Patch version
      uint16_t storage_lmq_port; // Storage Server lmq port to include in uptime proofs
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version_major);
        KV_SERIALIZE(version_minor);
        KV_SERIALIZE(version_patch);
        KV_SERIALIZE(storage_lmq_port);
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status; // Generic RPC error code. "OK" is the success value.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_LOKINET_PING
  {
    struct request
    {
      std::array<int, 3> version; // Lokinet version
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version);
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status; // Generic RPC error code. "OK" is the success value.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get the required amount of Loki to become a Service Node at the queried height. 
  // For stagenet and testnet values, ensure the daemon is started with the 
  // `--stagenet` or `--testnet` flags respectively.
  struct COMMAND_RPC_GET_STAKING_REQUIREMENT
  {
    struct request_t
    {
      uint64_t height; // The height to query the staking requirement for.  0 (or omitting) means current height.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      uint64_t staking_requirement; // The staking requirement in Loki, in atomic units.
      uint64_t height;              // The height requested (or current height if 0 was requested)
      std::string status;           // Generic RPC error code. "OK" is the success value.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(staking_requirement)
        KV_SERIALIZE(height)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get information on blacklisted Service Node key images.
  struct COMMAND_RPC_GET_SERVICE_NODE_BLACKLISTED_KEY_IMAGES
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP()
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct entry
    {
      std::string key_image;  // The key image of the transaction that is blacklisted on the network.
      uint64_t unlock_height; // The height at which the key image is removed from the blacklist and becomes spendable.
      uint64_t amount;        // The total amount of locked Loki in atomic units in this blacklisted stake.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(key_image)
        KV_SERIALIZE(unlock_height)
        KV_SERIALIZE(amount)
      END_KV_SERIALIZE_MAP()
    };

    struct response_t
    {
      std::vector<entry> blacklist; // Array of blacklisted key images, i.e. unspendable transactions
      std::string status;           // Generic RPC error code. "OK" is the success value.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blacklist)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get information on output blacklist.
  struct COMMAND_RPC_GET_OUTPUT_BLACKLIST
  {
    struct request_t
    {
      BEGIN_KV_SERIALIZE_MAP() 
      END_KV_SERIALIZE_MAP() 
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::vector<uint64_t> blacklist; // (Developer): Array of indexes from the global output list, corresponding to blacklisted key images.
      std::string status;              // Generic RPC error code. "OK" is the success value.
      bool untrusted;                  // If the result is obtained using bootstrap mode, and therefore not trusted `true`, or otherwise `false`.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(blacklist)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Query hardcoded/service node checkpoints stored for the blockchain. Omit all arguments to retrieve the latest "count" checkpoints.
  struct COMMAND_RPC_GET_CHECKPOINTS
  {
    constexpr static uint32_t NUM_CHECKPOINTS_TO_QUERY_BY_DEFAULT = 60;
    constexpr static uint64_t HEIGHT_SENTINEL_VALUE               = (UINT64_MAX - 1);
    struct request_t
    {
      uint64_t start_height; // Optional: Get the first count checkpoints starting from this height. Specify both start and end to get the checkpoints inbetween.
      uint64_t end_height;   // Optional: Get the first count checkpoints before end height. Specify both start and end to get the checkpoints inbetween.
      uint32_t count;        // Optional: Number of checkpoints to query.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE_OPT(start_height, HEIGHT_SENTINEL_VALUE)
        KV_SERIALIZE_OPT(end_height, HEIGHT_SENTINEL_VALUE)
        KV_SERIALIZE_OPT(count, NUM_CHECKPOINTS_TO_QUERY_BY_DEFAULT)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct voter_to_signature_serialized
    {
      uint16_t voter_index;  // Index of the voter in the relevant quorum
      std::string signature; // The signature generated by the voter in the quorum

      voter_to_signature_serialized() = default;
      voter_to_signature_serialized(service_nodes::voter_to_signature const &entry)
      : voter_index(entry.voter_index)
      , signature(epee::string_tools::pod_to_hex(entry.signature)) { }

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(voter_index);
        KV_SERIALIZE(signature);
      END_KV_SERIALIZE_MAP()

      BEGIN_SERIALIZE() // NOTE: For store_t_to_json
        FIELD(voter_index)
        FIELD(signature)
      END_SERIALIZE()
    };

    struct checkpoint_serialized
    {
      uint8_t version;
      std::string type;                                      // Either "Hardcoded" or "ServiceNode" for checkpoints generated by Service Nodes or declared in the code
      uint64_t height;                                       // The height the checkpoint is relevant for
      std::string block_hash;                                // The block hash the checkpoint is specifying
      std::vector<voter_to_signature_serialized> signatures; // Signatures from Service Nodes who agree on the block hash
      uint64_t prev_height;                                  // The previous height the checkpoint is based off

      checkpoint_serialized() = default;
      checkpoint_serialized(checkpoint_t const &checkpoint)
      : version(checkpoint.version)
      , type(checkpoint_t::type_to_string(checkpoint.type))
      , height(checkpoint.height)
      , block_hash(epee::string_tools::pod_to_hex(checkpoint.block_hash))
      , prev_height(checkpoint.prev_height)
      {
        signatures.reserve(checkpoint.signatures.size());
        for (service_nodes::voter_to_signature const &entry : checkpoint.signatures)
          signatures.push_back(entry);
      }

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(version);
        KV_SERIALIZE(type);
        KV_SERIALIZE(height);
        KV_SERIALIZE(block_hash);
        KV_SERIALIZE(signatures);
        KV_SERIALIZE(prev_height);
      END_KV_SERIALIZE_MAP()

      BEGIN_SERIALIZE() // NOTE: For store_t_to_json
        FIELD(version)
        FIELD(type)
        FIELD(height)
        FIELD(block_hash)
        FIELD(signatures)
        FIELD(prev_height)
      END_SERIALIZE()
    };

    struct response_t
    {
      std::vector<checkpoint_serialized> checkpoints; // Array of requested checkpoints
      std::string status;                             // Generic RPC error code. "OK" is the success value.
      bool untrusted;                                 // If the result is obtained using bootstrap mode, and therefore not trusted `true`, or otherwise `false`.

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(checkpoints)
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };

  LOKI_RPC_DOC_INTROSPECT
  // Query hardcoded/service node checkpoints stored for the blockchain. Omit all arguments to retrieve the latest "count" checkpoints.
  struct COMMAND_RPC_GET_SN_STATE_CHANGES
  {
    constexpr static uint32_t NUM_BLOCKS_TO_SCAN_BY_DEFAULT = 720;
    constexpr static uint64_t HEIGHT_SENTINEL_VALUE         = (UINT64_MAX - 1);
    struct request_t
    {
      uint64_t start_height;
      uint64_t end_height;   // Optional: If omitted, the tally runs until the current block

      BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(start_height)
      KV_SERIALIZE_OPT(end_height, HEIGHT_SENTINEL_VALUE)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<request_t> request;

    struct response_t
    {
      std::string status;                    // Generic RPC error code. "OK" is the success value.
      bool untrusted;                        // If the result is obtained using bootstrap mode, and therefore not trusted `true`, or otherwise `false`.

      uint32_t total_deregister;
      uint32_t total_ip_change_penalty;
      uint32_t total_decommission;
      uint32_t total_recommission;
      uint32_t total_unlock;
      uint64_t start_height;
      uint64_t end_height;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
        KV_SERIALIZE(untrusted)
        KV_SERIALIZE(total_deregister)
        KV_SERIALIZE(total_ip_change_penalty)
        KV_SERIALIZE(total_decommission)
        KV_SERIALIZE(total_recommission)
        KV_SERIALIZE(start_height)
        KV_SERIALIZE(end_height)
      END_KV_SERIALIZE_MAP()
    };
    typedef epee::misc_utils::struct_init<response_t> response;
  };


  LOKI_RPC_DOC_INTROSPECT
  struct COMMAND_RPC_REPORT_PEER_SS_STATUS
  {
    struct request
    {
      std::string type; // test type (currently used: ["reachability"])
      std::string pubkey; // service node pubkey
      bool passed; // whether the node is passing the test
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(type)
        KV_SERIALIZE(pubkey)
        KV_SERIALIZE(passed)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::string status; // Generic RPC error code. "OK" is the success value.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  // Deliberately undocumented; this RPC call is really only useful for testing purposes to reset
  // the resync idle timer (which normally fires every 60s) for the test suite.
  struct COMMAND_RPC_TEST_TRIGGER_P2P_RESYNC
  {
    struct request { BEGIN_KV_SERIALIZE_MAP() END_KV_SERIALIZE_MAP() };
    struct response {
      std::string status;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get the name mapping for a Loki Name Service entry. Loki currently supports mappings
  // for Session.
  struct COMMAND_RPC_LNS_NAMES_TO_OWNERS
  {
    static size_t const MAX_REQUEST_ENTRIES      = 256;
    static size_t const MAX_TYPE_REQUEST_ENTRIES = 8;
    struct request_entry
    {
      std::string name_hash; // The name hashed using libsodium's crypto_generichash_blake2b in base64 to resolve to a public key via Loki Name Service
      std::vector<uint16_t> types; // If empty, query all types. Currently only Session(0). In future updates more mapping types will be available.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(name_hash)
        KV_SERIALIZE(types)
      END_KV_SERIALIZE_MAP()
    };

    struct request
    {
      std::vector<request_entry> entries;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(entries)
      END_KV_SERIALIZE_MAP()
    };

    struct response_entry
    {
      uint64_t entry_index;     // The index in request_entry's `entries` array that was resolved via Loki Name Service.
      uint16_t type;            // The type of Loki Name Service entry that the owner owns.
      std::string name_hash;    // The hash of the name that was queried in base64
      std::string owner;        // The public key that purchased the Loki Name Service entry.
      std::string backup_owner; // The backup public key that the owner specified when purchasing the Loki Name Service entry.
      std::string encrypted_value; // The encrypted value that the name maps to. This value is encrypted using the name (not the hash) as the secret.
      uint64_t register_height; // The height that this Loki Name Service entry was purchased on the Blockchain.
      uint64_t update_height;   // The last height that this Loki Name Service entry was updated on the Blockchain.
      std::string txid;         // The txid of who purchased the mapping, null hash if not applicable.
      std::string prev_txid;    // The previous txid that purchased the mapping, null hash if not applicable.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(entry_index)
        KV_SERIALIZE(type)
        KV_SERIALIZE(name_hash)
        KV_SERIALIZE(owner)
        KV_SERIALIZE(backup_owner)
        KV_SERIALIZE(encrypted_value)
        KV_SERIALIZE(register_height)
        KV_SERIALIZE(update_height)
        KV_SERIALIZE(txid)
        KV_SERIALIZE(prev_txid)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::vector<response_entry> entries;
      std::string status; // Generic RPC error code. "OK" is the success value.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(entries)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };

  LOKI_RPC_DOC_INTROSPECT
  // Get all the name mappings for the queried owner. The owner can be either a ed25519 public key or Monero style
  // public key; by default purchases are owned by the spend public key of the purchasing wallet.
  struct COMMAND_RPC_LNS_OWNERS_TO_NAMES
  {
    static size_t const MAX_REQUEST_ENTRIES = 256;
    struct request
    {
      std::vector<std::string> entries; // The owner's public key to find all Loki Name Service entries for.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(entries)
      END_KV_SERIALIZE_MAP()
    };

    struct response_entry
    {
      uint64_t    request_index;   // (Deprecated) The index in request's `entries` array that was resolved via Loki Name Service.
      uint16_t    type;            // The category the Loki Name Service entry belongs to, currently only Session whose value is 0.
      std::string name_hash;       // The hash of the name that the owner purchased via Loki Name Service in base64
      std::string owner;           // The backup public key specified by the owner that purchased the Loki Name Service entry.
      std::string backup_owner;    // The backup public key specified by the owner that purchased the Loki Name Service entry.
      std::string encrypted_value; // The encrypted value that the name maps to. This value is encrypted using the name (not the hash) as the secret.
      uint64_t    register_height; // The height that this Loki Name Service entry was purchased on the Blockchain.
      uint64_t    update_height;   // The last height that this Loki Name Service entry was updated on the Blockchain.
      std::string txid;            // The txid of who purchases the mapping.
      std::string prev_txid;       // The previous txid that purchased the mapping, null hash if not applicable.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(request_index)
        KV_SERIALIZE(type)
        KV_SERIALIZE(name_hash)
        KV_SERIALIZE(owner)
        KV_SERIALIZE(backup_owner)
        KV_SERIALIZE(encrypted_value)
        KV_SERIALIZE(register_height)
        KV_SERIALIZE(update_height)
        KV_SERIALIZE(txid)
        KV_SERIALIZE(prev_txid)
      END_KV_SERIALIZE_MAP()
    };

    struct response
    {
      std::vector<response_entry> entries;
      std::string status; // Generic RPC error code. "OK" is the success value.
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(entries)
        KV_SERIALIZE(status)
      END_KV_SERIALIZE_MAP()
    };
  };
}
