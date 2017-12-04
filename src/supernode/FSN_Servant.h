#ifndef FSN_SERVANT_H_
#define FSN_SERVANT_H_

#include "supernode_common_struct.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <string>
using namespace std;

namespace supernode {

	class FSN_Servant {
		public:
		// data for my wallet access
		void Set(const string& stakeFileName, const string& stakePasswd, const string& minerFileName, const string& minerPasswd);

		public:
		// next two fields may be references
		mutable boost::mutex All_FSN_Guard;// DO NOT block for long time. if need - use copy
		vector< boost::shared_ptr<FSN_Data> > All_FSN;// access to this data may be done from different threads

		// start from blockchain top and check, if block solved by one from  full_super_node_servant::all_fsn
		// push block number and full_super_node_data to output vector. stop, when output_vector.size==blockNums OR blockchain ends
		// output_vector.begin - newest block, solved by FSN
		// may be called from different threads.
		vector< pair<uint64_t, boost::shared_ptr<FSN_Data> > > LastBlocksResolvedByFSN(uint64_t startFromBlock, uint64_t blockNums) const;

		// start scan blockchain from forBlockNum, scan from top to bottom
		vector< boost::shared_ptr<FSN_Data> > GetAuthSample(uint64_t forBlockNum) const;
		uint64_t GetCurrentBlockNum() const;


		string SignByWalletPrivateKey(const string& str, const string& wallet_addr) const;
		bool IsSignValid(const string& str, const FSN_WalletData& wallet) const;

		// calc balance from chain begin to block_num
		unsigned GetWalletBalance(uint64_t block_num, const FSN_WalletData& wallet) const;

		public:
		FSN_WalletData GetMyStakeWallet() const;
		FSN_WalletData GetMyMinerWallet() const;

		static const unsigned FSN_PerAuthSample = 8;
	};

};

#endif /* FSN_SERVANT_H_ */
