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


#include "rta_helpers_gui.h"
#include "rta_helpers.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include <cryptonote_basic/cryptonote_format_utils.h>
#include "misc_log_ex.h"
#include "utils.h"
#include "string_tools.h"

#include <vector>
#include <utility>

namespace graft {
namespace rta_helpers {
namespace gui {

bool decrypt_tx_and_amount(const std::string &wallet_address, size_t nettype, const crypto::secret_key &key, const std::string &encrypted_tx_key, 
                             const std::string &encrypted_tx, uint64_t &amount, std::string &tx_blob, std::string &error)
{
    // parse wallet address
    std::ostringstream oss;
    
    cryptonote::address_parse_info parse_info;
    if (!cryptonote::get_account_address_from_str(parse_info, static_cast<cryptonote::network_type>(nettype), wallet_address)) {
        oss << "Failed to parse account from: " << wallet_address;
        MERROR(oss.str());
        error = oss.str();
        return false;
    }
    
    cryptonote::transaction tx;
    
    if (!graft::rta_helpers::decryptTxFromHex(encrypted_tx, key, tx)) {
        MERROR("Failed to decrypt tx from encrypted hex: " << encrypted_tx);
        return false;
    }
    
    tx_blob = cryptonote::tx_to_blob(tx);
    crypto::secret_key tx_key;
    
    if (!graft::rta_helpers::decryptTxKeyFromHex(encrypted_tx_key, key, tx_key)) {
        MERROR("Failed to decrypt tx key from encrypted hex: " << encrypted_tx_key);
        return false;
    }
    
    std::vector<std::pair<size_t, uint64_t>> outputs;
    
    if (!Utils::get_tx_amount(parse_info.address, tx_key, tx, outputs, amount)) {
        MERROR("Failed to get amount from tx");
        return false;
    }
    
    return true;
}

bool pos_approve_tx(const std::string tx_blob, const crypto::public_key &pkey, const crypto::secret_key &skey, size_t auth_sample_size,
                    std::string &out_encrypted_tx_hex)
{
    
    cryptonote::transaction tx;
    
    if (!cryptonote::parse_and_validate_tx_from_blob(tx_blob, tx)) {
        MERROR("Failed to parse transaction from blob");
        return false;
    }
    
    std::vector<cryptonote::rta_signature> rta_signatures;
    cryptonote::rta_header rta_hdr;

    if (!cryptonote::get_graft_rta_header_from_extra(tx, rta_hdr)) {
        MERROR("Failed to read rta_hdr from tx");
        return false;
    }

    rta_signatures.resize(auth_sample_size + 3);
    crypto::signature &sig = rta_signatures.at(cryptonote::rta_header::POS_KEY_INDEX).signature;
    crypto::hash hash = cryptonote::get_transaction_hash(tx);
    crypto::generate_signature(hash, pkey, skey, sig);

    tx.extra2.clear();
    cryptonote::add_graft_rta_signatures_to_extra2(tx.extra2, rta_signatures);
    graft::rta_helpers::encryptTxToHex(tx, rta_hdr.keys, out_encrypted_tx_hex);

    return true;
}

bool get_rta_keys_from_tx(const std::string &tx_blob, std::vector<std::string> &rta_keys)
{
    cryptonote::transaction tx;
    
    if (!cryptonote::parse_and_validate_tx_from_blob(tx_blob, tx)) {
        MERROR("Failed to parse transaction from blob");
        return false;
    }
    
    std::vector<cryptonote::rta_signature> rta_signatures;
    cryptonote::rta_header rta_hdr;

    if (!cryptonote::get_graft_rta_header_from_extra(tx, rta_hdr)) {
        MERROR("Failed to read rta_hdr from tx");
        return false;
    }

    rta_keys.clear();
    for (const auto &key : rta_hdr.keys) {
        rta_keys.push_back(epee::string_tools::pod_to_hex(key));
    }
    return true;
}

    

}}}
