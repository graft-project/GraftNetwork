#include "supernode_common_struct.h"

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
	bool bb = s.Sum!=Sum || s.POS_Wallet!=POS_Wallet || s.BlockNum!=BlockNum || s.PaymentID!=PaymentID;
	if(bb) return false;

	if( s.AuthNodes.size()!=AuthNodes.size() ) return false;

	for(unsigned i=0;i<AuthNodes.size();i++) {
		if( *AuthNodes[i]!=*s.AuthNodes[i] ) return false;
	}


	return true;

}

};
