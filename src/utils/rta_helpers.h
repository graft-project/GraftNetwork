// Copyright (c) 2020, The Graft Project
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

#ifndef RTA_HELPERS_H
#define RTA_HELPERS_H

#include <crypto/crypto.h>
#include <string>
#include <cryptonote_basic/cryptonote_basic.h>

namespace graft {
namespace rta_helpers {

/*!
 * \brief decryptTxFromHex  - decrypts transaction from encrypted hexadecimal string
 * \param encryptedHex  - input data as hex string
 * \param key           - decryption secret key
 * \param tx            - output tx
 * \return              - true on success
 */
bool decryptTxFromHex(const std::string &encryptedHex, const crypto::secret_key &key, cryptonote::transaction &tx);

/*!
 * \brief encryptTxToHex - encrypts transaction using public keys (one-to-many scheme)
 * \param tx             - transaction to encrypt
 * \param keys           - keys
 * \param encryptedHex   - output encoded as hexadecimal string
 */
void encryptTxToHex(const cryptonote::transaction &tx, const std::vector<crypto::public_key> &keys, std::string &encryptedHex);

/*!
 * \brief decryptTxKeyFromHex - decrypts transaction key from encrypted hexadecimal string
 * \param encryptedHex        - input data as hex string
 * \param key                 - decryption key
 * \param tx_key              - output tx key
 * \return                    - true on success
 */
bool decryptTxKeyFromHex(const std::string &encryptedHex, const crypto::secret_key &key, crypto::secret_key &tx_key);

/*!
 * \brief encryptTxKeyToHex - encrypts tx key using public keys (one-to-many scheme)
 * \param tx_key            - tx key to encrypt
 * \param keys              - public keys to encrypt
 * \param encryptedHex      - output hex string
 */
void encryptTxKeyToHex(const crypto::secret_key &tx_key, const std::vector<crypto::public_key> &keys, std::string &encryptedHex);


}
}


#endif // RTA_HELPERS_H
