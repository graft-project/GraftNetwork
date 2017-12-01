#ifndef SUPERNODE_COMMON_STRUCT_H_
#define SUPERNODE_COMMON_STRUCT_H_

#include <uuid/uuid.h>
#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>
#include <inttypes.h>
using namespace std;

#define LOG_PRINT_L5(xx) { cout<<xx<<endl; }

namespace supernode {

	// ------------------------------------------
	struct FSN_WalletData {
		string Addr;
		string ViewKey;
	};


	// ------------------------------------------
	struct FSN_Data {
		FSN_WalletData Stake;
		FSN_WalletData Miner;
		string IP;
		string Port;
	};


	// ------------------------------------------
	struct SubNetData {
		uuid_t PaymentID;
	};


	// ------------------------------------------
	struct RTA_TransactionRecordBase : public SubNetData {
		unsigned Sum;
		string POS_Wallet;
		string DataForClientWallet;// empty in wallet call
		uint64_t BlockNum;// empty in pos call
	};


	// ------------------------------------------
	struct RTA_TransactionRecord : public RTA_TransactionRecordBase {
		bool operator!=(const RTA_TransactionRecord& s) const {
			// TODO: IMPL
			return true;
		}
		vector< boost::shared_ptr<FSN_Data> > AuthNodes;
	};



};




#endif /* SUPERNODE_COMMON_STRUCT_H_ */
