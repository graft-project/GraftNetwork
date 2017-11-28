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

#pragma once
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "crypto/hash.h"


namespace tools {
namespace supernode_rpc {

	struct COMMAND_RPC_EMPTY_TEST {
		struct request {
			BEGIN_KV_SERIALIZE_MAP()
			END_KV_SERIALIZE_MAP()
		};

		struct response {
			BEGIN_KV_SERIALIZE_MAP()
			END_KV_SERIALIZE_MAP()
		};
	};

    //Wallet DAPI
    struct COMMAND_RPC_READY_TO_PAY
    {
      struct request
      {
        std::string pid;
        std::string key_image;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(pid)
          KV_SERIALIZE(key_image)
        END_KV_SERIALIZE_MAP()
      };

      struct response
      {
        int64_t result;
        std::string transaction;
        std::string data;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(result)
          KV_SERIALIZE(transaction)
          KV_SERIALIZE(data)
        END_KV_SERIALIZE_MAP()
      };
    };

    struct COMMAND_RPC_REJECT_PAY
    {
      struct request
      {
        std::string pid;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(pid)
        END_KV_SERIALIZE_MAP()
      };

      struct response
      {
        int64_t result;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(result)
        END_KV_SERIALIZE_MAP()
      };
    };

    struct transaction_record
    {
      uint64_t amount;
      std::string address;
      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(amount)
        KV_SERIALIZE(address)
      END_KV_SERIALIZE_MAP()
    };

    struct COMMAND_RPC_PAY
    {
      struct request
      {
        std::string pid;
        transaction_record transaction;
        std::string account;
        std::string password;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(pid)
          KV_SERIALIZE(transaction)
          KV_SERIALIZE(account)
          KV_SERIALIZE(password)
        END_KV_SERIALIZE_MAP()
      };

      struct response
      {
        int64_t result;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(result)
        END_KV_SERIALIZE_MAP()
      };
    };

    struct COMMAND_RPC_GET_PAY_STATUS
    {
      struct request
      {
        std::string pid;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(pid)
        END_KV_SERIALIZE_MAP()
      };

      struct response
      {
        int64_t result;
        int64_t pay_status;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(result)
          KV_SERIALIZE(pay_status)
        END_KV_SERIALIZE_MAP()
      };
    };

    //Point of Sale DAPI
    struct COMMAND_RPC_SALE
    {
      struct request
      {
        std::string pid;
        std::string data;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(pid)
          KV_SERIALIZE(data)
        END_KV_SERIALIZE_MAP()
      };

      struct response
      {
        int64_t result;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(result)
        END_KV_SERIALIZE_MAP()
      };
    };

    struct COMMAND_RPC_GET_SALE_STATUS
    {
      struct request
      {
        std::string pid;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(pid)
        END_KV_SERIALIZE_MAP()
      };

      struct response
      {
        int64_t result;
        int64_t sale_status;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(result)
          KV_SERIALIZE(sale_status)
        END_KV_SERIALIZE_MAP()
      };
    };

    //Generic DAPI
    struct COMMAND_RPC_GET_WALLET_BALANCE
    {
      struct request
      {
        std::string account;
        std::string password;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(account)
          KV_SERIALIZE(password)
        END_KV_SERIALIZE_MAP()
      };

      struct response
      {
        uint64_t balance;
        uint64_t unlocked_balance;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(balance)
          KV_SERIALIZE(unlocked_balance)
        END_KV_SERIALIZE_MAP()
      };
    };

    struct COMMAND_RPC_GET_SUPERNODE_LIST
    {
      struct request
      {
        BEGIN_KV_SERIALIZE_MAP()
        END_KV_SERIALIZE_MAP()
      };

      struct response
      {
        int64_t result;
        std::list<std::string> supernode_list;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(result)
          KV_SERIALIZE(supernode_list)
        END_KV_SERIALIZE_MAP()
      };
    };

    //Temporal DAPI
    struct COMMAND_RPC_CREATE_ACCOUNT
    {
      struct request
      {
        std::string password;
        std::string language;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(password)
          KV_SERIALIZE(language)
        END_KV_SERIALIZE_MAP()
      };
      struct response
      {
        std::string account;
        std::string address;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(account)
          KV_SERIALIZE(address)
        END_KV_SERIALIZE_MAP()
      };
    };

    struct COMMAND_RPC_GET_PAYMENT_ADDRESS
    {
      struct request
      {
          std::string account;
          std::string password;
          std::string payment_id;

          BEGIN_KV_SERIALIZE_MAP()
            KV_SERIALIZE(account)
            KV_SERIALIZE(password)
            KV_SERIALIZE(payment_id)
          END_KV_SERIALIZE_MAP()
      };
      struct response
      {
          std::string payment_address;
          std::string payment_id;

          BEGIN_KV_SERIALIZE_MAP()
            KV_SERIALIZE(payment_address)
            KV_SERIALIZE(payment_id)
          END_KV_SERIALIZE_MAP()
      };
    };
}
}
