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
        FSN_WalletData() = default;
        FSN_WalletData(const FSN_WalletData &other) = default;
        FSN_WalletData(const std::string &_addr, const std::string &_viewkey)
            : Addr{_addr}
            , ViewKey{_viewkey} {}



        string Addr;
		string ViewKey;
	};


	// ------------------------------------------
	struct FSN_Data {
        FSN_Data(const FSN_WalletData &_stakeWallet, const FSN_WalletData &_minerWallet,
                 const std::string &_ip = "", const std::string &_port = "")
            : Stake{_stakeWallet}
            , Miner{_minerWallet}
            , IP{_ip}
            , Port{_port} {}


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
