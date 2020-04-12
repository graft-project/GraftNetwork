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


#ifndef RTA_HELPERS_GUI_H
#define RTA_HELPERS_GUI_H

#include <crypto/crypto.h>
#include <string>

namespace graft {
namespace rta_helpers {
namespace gui {

/*!
 * \brief decrypt_tx_and_amount - retreives tx amount sent to 'wallet_address' from encrypted tx
 * \param wallet_address          - destination address
 * \param key                     - decryption key
 * \param encrypted_tx_key        - encrypted tx key blob  as hex
 * \param encrypted_tx            - encrypted tx blob as hex
 * \param amount                  - output amount in atomic units
 * \param tx_blob                 - output decrypted tx blob
 * \return                        - true on success
 */

bool decrypt_tx_and_amount(const std::string &wallet_address, size_t nettype, const crypto::secret_key &key, const std::string &encrypted_tx_key, 
                            const std::string &encrypted_tx, uint64_t &amount, std::string &tx_blob);

/*!
 * \brief get_rta_keys_from_tx - returns rta keys from tx
 * \param tx_blob              - serialized tx
 * \param rta_keys             - out vector with hexadecimal keys
 * \return 
 */
bool get_rta_keys_from_tx(const std::string &tx_blob, std::vector<std::string> &rta_keys);




bool pos_approve_tx(const std::string tx_blob, const crypto::public_key &pkey, const crypto::secret_key &skey, size_t auth_sample_size, std::string &out_encrypted_tx_hex);


} } }
#endif // RTA_HELPERS_GUI_H
