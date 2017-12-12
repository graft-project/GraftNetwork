#ifndef FSN_SERVANTBASE_H_H_H_
#define FSN_SERVANTBASE_H_H_H_

#include "supernode_common_struct.h"

namespace supernode {
	class FSN_ServantBase {
	public:
		virtual ~FSN_ServantBase();

	    virtual vector<pair<uint64_t, boost::shared_ptr<FSN_Data>>>
	    LastBlocksResolvedByFSN(uint64_t startFromBlock, uint64_t blockNums) const=0;

	    virtual vector<boost::shared_ptr<FSN_Data>> GetAuthSample(uint64_t forBlockNum) const=0;

	    virtual uint64_t GetCurrentBlockHeight() const=0;

	    virtual string SignByWalletPrivateKey(const string& str, const string& wallet_addr) const=0;

	    virtual bool IsSignValid(const string& message, const string &address, const string &signature) const=0;


	    virtual uint64_t GetWalletBalance(uint64_t block_num, const FSN_WalletData& wallet) const=0;

	public:
	    virtual void AddFsnAccount(boost::shared_ptr<FSN_Data> fsn);
	    virtual bool RemoveFsnAccount(boost::shared_ptr<FSN_Data> fsn);
	    virtual boost::shared_ptr<FSN_Data> FSN_DataByStakeAddr(const string& addr) const;

	public:
	    virtual FSN_WalletData GetMyStakeWallet() const=0;
	    virtual FSN_WalletData GetMyMinerWallet() const=0;
	    virtual unsigned AuthSampleSize() const=0;


	protected:
	    // next two fields may be references
	    mutable boost::recursive_mutex All_FSN_Guard;// DO NOT block for long time. if need - use copy
	    // TODO: store FSN_Data and corresponding wallet in single map
	    vector< boost::shared_ptr<FSN_Data> > All_FSN;// access to this data may be done from different threads



	};


};



#endif
