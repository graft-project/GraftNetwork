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

#include "supernode_common_struct.h"
#include <boost/lexical_cast.hpp>

namespace supernode {




bool FSN_WalletData::operator!=(const FSN_WalletData& s) const { return !(*this==s); }
bool FSN_WalletData::operator==(const FSN_WalletData& s) const {
	return s.Addr==Addr && s.ViewKey==ViewKey;
}


bool FSN_Data::operator!=(const FSN_Data& s) const { return !(*this==s); }

bool FSN_Data::operator==(const FSN_Data& s) const {
	return IP==s.IP && Port==s.Port && Stake==s.Stake && Miner==s.Miner;
}


bool RTA_TransactionRecord::operator!=(const RTA_TransactionRecord& s) const { return !(*this==s); }

bool RTA_TransactionRecord::operator==(const RTA_TransactionRecord& s) const {
    bool bb = s.Amount != Amount || s.POSAddress!=POSAddress || s.BlockNum!=BlockNum || s.PaymentID!=PaymentID;
	if(bb) return false;

	if( s.AuthNodes.size()!=AuthNodes.size() ) return false;

	for(unsigned i=0;i<AuthNodes.size();i++) {
		if( *AuthNodes[i]!=*s.AuthNodes[i] ) return false;
	}


	return true;

}

string RTA_TransactionRecord::MessageForSign() const {
	return boost::lexical_cast<string>(Amount)+string("-")+boost::lexical_cast<string>(BlockNum)+string("-")+PaymentID;
}

};
