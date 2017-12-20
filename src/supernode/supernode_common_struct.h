#ifndef SUPERNODE_COMMON_STRUCT_H_
#define SUPERNODE_COMMON_STRUCT_H_

#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>
#include <inttypes.h>
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "crypto/hash.h"
using namespace std;

#define LOG_PRINT_L5(xx) { cout<<__FILE__<<" : "<<xx<<endl; }

namespace supernode {

    enum class NTransactionStatus : int {
		None = 0,
		InProgress=1,
		Success=2,
		Fail=3,
        RejectedByWallet=4,
        RejectedByPOS=5
	};


	// ------------------------------------------
	struct FSN_WalletData {
        FSN_WalletData() = default;
        FSN_WalletData(const FSN_WalletData &other) = default;
        FSN_WalletData(const std::string &_addr, const std::string &_viewkey)
            : Addr{_addr}
            , ViewKey{_viewkey} {}

        bool operator!=(const FSN_WalletData& s) const;
        bool operator==(const FSN_WalletData& s) const;


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
        FSN_Data() {}

        bool operator!=(const FSN_Data& s) const;
        bool operator==(const FSN_Data& s) const;
		FSN_WalletData Stake;
		FSN_WalletData Miner;
		string IP;
		string Port;
	};


	// ------------------------------------------
	struct SubNetData {
		BEGIN_KV_SERIALIZE_MAP()
			KV_SERIALIZE(PaymentID)
		END_KV_SERIALIZE_MAP()

		string PaymentID;
	};


	// ------------------------------------------
	struct RTA_TransactionRecordBase : public SubNetData {
        unsigned Amount;
        string POSAddress;
        string POSSaleDetails;// empty in wallet call
        uint64_t BlockNum;// empty in pos call
	};


	// ------------------------------------------
	struct RTA_TransactionRecord : public RTA_TransactionRecordBase {
		bool operator!=(const RTA_TransactionRecord& s) const;
		bool operator==(const RTA_TransactionRecord& s) const;
		string MessageForSign() const;
		vector< boost::shared_ptr<FSN_Data> > AuthNodes;
	};



};




#endif /* SUPERNODE_COMMON_STRUCT_H_ */
