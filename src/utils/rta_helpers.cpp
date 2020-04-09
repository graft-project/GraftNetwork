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

#include "rta_helpers.h"
#include "misc_log_ex.h"
#include "string_tools.h"
#include "cryptmsg.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

namespace graft {
namespace rta_helpers {


bool decryptTxFromHex(const std::string &encryptedHex, const crypto::secret_key &key, cryptonote::transaction &tx)
{
    std::string decryptedTxBlob, encryptedTxBlob;

    if (!epee::string_tools::parse_hexstr_to_binbuff(encryptedHex, encryptedTxBlob)) {
        MERROR("failed to deserialize encrypted tx blob");
        return false;
    }

    if (!graft::crypto_tools::decryptMessage(encryptedTxBlob, key, decryptedTxBlob)) {
        MERROR("Failed to decrypt tx");
        return false;
    }

    if (!cryptonote::parse_and_validate_tx_from_blob(decryptedTxBlob, tx)) {
        MERROR("Failed to parse transaction from blob");
        return false;
    }
    return true;
}

void encryptTxToHex(const cryptonote::transaction &tx, const std::vector<crypto::public_key> &keys, std::string &encryptedHex)
{
    std::string buf;
    graft::crypto_tools::encryptMessage(cryptonote::tx_to_blob(tx), keys, buf);
    encryptedHex = epee::string_tools::buff_to_hex_nodelimer(buf);

}

bool decryptTxKeyFromHex(const std::string &encryptedHex, const crypto::secret_key &key, crypto::secret_key &tx_key)
{
    std::string decryptedBlob, encryptedBlob;
    if (!epee::string_tools::parse_hexstr_to_binbuff(encryptedHex, encryptedBlob)) {
        MERROR("failed to deserialize encrypted tx key blob");
        return false;
    }

    if (!graft::crypto_tools::decryptMessage(encryptedBlob, key, decryptedBlob)) {
        MERROR("Failed to decrypt tx");
        return false;
    }

    memcpy(&tx_key, decryptedBlob.c_str(), decryptedBlob.size());
    return true;
}

void encryptTxKeyToHex(const crypto::secret_key &tx_key, const std::vector<crypto::public_key> &keys, std::string &encryptedHex)
{
    std::string buf;
    std::string tx_buf(reinterpret_cast<const char*>(&tx_key), sizeof(crypto::secret_key));
    graft::crypto_tools::encryptMessage(tx_buf, keys, buf);
    encryptedHex = epee::string_tools::buff_to_hex_nodelimer(buf);
}

}
}
