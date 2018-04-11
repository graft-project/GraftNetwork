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
// Parts of this file are originally copyright (c) 2014-2017, The Monero Project

#pragma once


#include "wallet/wallet2.h"
#include "grafttxextra.h"
#include "wallet/api/wallet2_api.h"
#include <memory>
// #include <boost/optional.hpp>


//#undef MONERO_DEFAULT_LOG_CATEGORY
//#define MONERO_DEFAULT_LOG_CATEGORY "wallet.graftwallet"

class Serialization_portability_wallet_Test;

namespace tools
{

class GraftWallet : public wallet2
{
  friend class ::Serialization_portability_wallet_Test;
public:
  static bool verify(const std::string &message, const std::string &address, const std::string &signature, cryptonote::network_type nettype);
  static std::unique_ptr<GraftWallet> createWallet(const std::string &daemon_address = std::string(),
                                                   const std::string &daemon_host = std::string(),
                                                   int daemon_port = 0,
                                                   const std::string &daemon_login = std::string(),
                                                   cryptonote::network_type nettype = cryptonote::MAINNET , bool restricted = false);
  static std::unique_ptr<GraftWallet> createWallet(const std::string &account_data,
                                                   const std::string &password,
                                                   const std::string &daemon_address = std::string(),
                                                   const std::string &daemon_host = std::string(),
                                                   int daemon_port = 0,
                                                   const std::string &daemon_login = std::string(),
                                                   cryptonote::network_type = cryptonote::MAINNET, bool restricted = false);


  /*!
     * \brief  Generates a wallet or restores one.
     * \param  password       Password of wallet file
     * \param  recovery_param If it is a restore, the recovery key
     * \param  recover        Whether it is a restore
     * \param  two_random     Whether it is a non-deterministic wallet
     * \return                The secret key of the generated wallet
     */
  crypto::secret_key generate_graft(const std::string& password,
                                    const crypto::secret_key& recovery_param,
                                    bool recover, bool two_random);
  void load_graft(const std::string& data, const std::string& password, const std::string &cache_file);

  //Graft Test
  std::string store_keys_graft(const std::string& password, bool watch_only = false);
  bool load_keys_graft(const std::string& data, const std::string& password);

  /*!
       * \brief load_cache - loads cache from given filename.
       *                     wallet's private keys should be already loaded before this call
       * \param filename - filename pointing to the file with cache
       */
  void load_cache(const std::string &filename);
  /*!
       * \brief store_cache - stores only cache to the file. cache is encrypted using wallet's private keys
       * \param path - filename to store the cache
       */
  void store_cache(const std::string &filename);

  Monero::PendingTransaction * createTransaction(const std::string &dst_addr, const std::string &payment_id,
                                                     boost::optional<uint64_t> amount, uint32_t mixin_count,
                                                     const supernode::GraftTxExtra &graftExtra,
                                                     Monero::PendingTransaction::Priority priority = Monero::PendingTransaction::Priority_Low);



};
}
