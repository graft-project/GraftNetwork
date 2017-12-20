#ifndef FSN_Servant_Test_H_H__H_H
#define FSN_Servant_Test_H_H__H_H

#include "FSN_Servant.h"



namespace supernode {

struct FSN_Servant_Test : public FSN_Servant {
	public:
	FSN_Servant_Test(const string &bdb_path, const string &daemon_addr, const string &fsn_wallets_dir, bool testnet = false) :
		FSN_Servant(bdb_path, daemon_addr, fsn_wallets_dir, testnet) {}

	unsigned AuthSampleSize() const override { return All_FSN.size(); }
	vector<boost::shared_ptr<FSN_Data>> GetAuthSample(uint64_t forBlockNum) const override { return All_FSN; }
};


};









#endif
