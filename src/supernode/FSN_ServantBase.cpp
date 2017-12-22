#include "FSN_ServantBase.h"
#include <boost/algorithm/string.hpp>


namespace supernode {


boost::shared_ptr<FSN_Data> FSN_ServantBase::FSN_DataByStakeAddr(const string& addr) const {
	boost::lock_guard<boost::recursive_mutex> lock(All_FSN_Guard);
	for(auto a : All_FSN) if( a->Stake.Addr==addr ) return a;
    return nullptr;
}

string FSN_ServantBase::GetNodeIp() const
{
    return m_nodeIp;
}

int FSN_ServantBase::GetNodePort() const
{
    return m_nodePort;
}

string FSN_ServantBase::GetNodeAddress() const
{
    return m_nodeIp + ":" + std::to_string(m_nodePort);
}

bool FSN_ServantBase::IsTestnet() const
{
    return m_testnet;
}

void FSN_ServantBase::SetNodeAddress(const string &address)
{
    vector<string> results;
    boost::algorithm::split(results, address, boost::is_any_of(":"));
    if (results.size() == 3) // address with protocol prefix (http://localhost:1234)
        results.erase(results.begin());

    if (results.size() == 2) {
        m_nodeIp = results[0];
        m_nodePort = std::stoi(results[1]);
    }
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



}
