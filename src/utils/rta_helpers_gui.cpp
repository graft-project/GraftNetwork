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
#include "misc_log_ex.h"
#include "utils.h"

#include <vector>
#include <utility>

namespace graft {
namespace rta_helpers {
namespace gui {

bool get_encrypted_tx_amount(const std::string &wallet_address, size_t nettype, const crypto::secret_key &key, const std::string &encrypted_tx_key, 
                             const std::string &encrypted_tx, uint64_t &amount)
{
    // parse wallet address
    cryptonote::address_parse_info parse_info;
    if (!cryptonote::get_account_address_from_str(parse_info, static_cast<cryptonote::network_type>(nettype), wallet_address)) {
        MERROR("Failed to parse account from: " << wallet_address);
        return false;
    }
    
    cryptonote::transaction tx;
    
    if (!graft::rta_helpers::decryptTxFromHex(encrypted_tx, key, tx)) {
        MERROR("Failed to decrypt tx from encrypted hex: " << encrypted_tx);
        return false;
    }
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
    

}}}
