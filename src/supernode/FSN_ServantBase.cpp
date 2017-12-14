#include "FSN_ServantBase.h"

namespace supernode {


boost::shared_ptr<FSN_Data> FSN_ServantBase::FSN_DataByStakeAddr(const string& addr) const {
	boost::lock_guard<boost::recursive_mutex> lock(All_FSN_Guard);
	for(auto a : All_FSN) if( a->Stake.Addr==addr ) return a;
	return nullptr;
}

void FSN_ServantBase::AddFsnAccount(boost::shared_ptr<FSN_Data> fsn) {
	boost::lock_guard<boost::recursive_mutex> lock(All_FSN_Guard);
    All_FSN.push_back(fsn);
}

bool FSN_ServantBase::RemoveFsnAccount(boost::shared_ptr<FSN_Data> fsn) {
	boost::lock_guard<boost::recursive_mutex> lock(All_FSN_Guard);
    const auto & it = std::find_if(All_FSN.begin(), All_FSN.end(),
                                   [fsn] (const boost::shared_ptr<FSN_Data> &other) {
                    return *fsn == *other;
            });

    if(it==All_FSN.end()) return false;
    All_FSN.erase(it);
    return true;
}



};
