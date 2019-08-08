// Copyright (c) 2019, The Graft Project
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

#pragma once

#include "crypto/crypto.h"
#include <vector>

namespace graft { namespace crypto_tools {

/*!
 * \brief encryptMessage - encrypts data for recipients using their B public keys (assumed public view keys).
 *
 * \param input - data to encrypt.
 * \param Bkeys - vector of B keys for each recipients.
 * \param output - resulting encripted message.
 */
void encryptMessage(const std::string& input, const std::vector<crypto::public_key>& Bkeys, std::string& output);

/*!
 * \brief encryptMessage - encrypts data for single recipient using B public keys (assumed public view key).
 *
 * \param input - data to encrypt.
 * \param Bkey - B keys of recipient.
 * \param output - resulting encripted message.
 */
void encryptMessage(const std::string& input, const crypto::public_key& Bkey, std::string& output);

/*!
 * \brief decryptMessage - (reverse of encryptMessage) decrypts data for one of the recipients using his secret key b.
 *
 * \param input - data that was created by encryptForBs.
 * \param bkey - secret key corresponding to one of Bs that were used to encrypt.
 * \param output - resulting decrypted data.
 * \return true on success or false otherwise
 */
bool decryptMessage(const std::string& input, const crypto::secret_key& bkey, std::string& output);

/*!
 * \brief hasPublicKey - check if a message created by encryptMessage has an entry for recipient with public key B.
 *
 * \param input - data that was created by encryptForBs.
 * \param Bkey - public key corresponding to one of Bs that were used to encrypt.
 * \return true if found
 */

bool hasPublicKey(const std::string& input, const crypto::public_key& Bkey);

}} //namespace graft::crypto_tools
