#include "FSN_Servant.h"


void supernode::FSN_Servant::Set(const string& stakeFileName, const string& stakePasswd, const string& minerFileName, const string& minerPasswd) {}
vector< pair<uint64_t, boost::shared_ptr<supernode::FSN_Data> > > supernode::FSN_Servant::LastBlocksResolvedByFSN(uint64_t startFromBlock, uint64_t blockNums) const { return vector< pair<uint64_t, boost::shared_ptr<supernode::FSN_Data> > >(); }

vector< boost::shared_ptr<supernode::FSN_Data> > supernode::FSN_Servant::GetAuthSample(uint64_t forBlockNum) const { return vector< boost::shared_ptr<supernode::FSN_Data> >(); }
uint64_t supernode::FSN_Servant::GetCurrentBlockNum() const { return 0; }


string supernode::FSN_Servant::SignByWalletPrivateKey(const string& str, const string& wallet_addr) const { return ""; }
bool supernode::FSN_Servant::IsSignValid(const string& str, const FSN_WalletData& wallet) const { return false; }

unsigned supernode::FSN_Servant::GetWalletBalance(uint64_t block_num, const FSN_WalletData& wallet) const { return 0; }

supernode::FSN_WalletData supernode::FSN_Servant::GetMyStakeWallet() const { return FSN_WalletData(); }
supernode::FSN_WalletData supernode::FSN_Servant::GetMyMinerWallet() const { return FSN_WalletData(); }

