// Copyright (c) 2017, The Graft Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "FSN_ServantBase.h"
#include <boost/algorithm/string.hpp>

namespace supernode {

boost::shared_ptr<FSN_Data> FSN_ServantBase::FSN_DataByStakeAddr(const string& addr) const
{
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

string FSN_ServantBase::GetNodeLogin() const
{
    return m_nodelogin;
}

string FSN_ServantBase::GetNodePassword() const
{
    return m_nodePassword;
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

    if (results.size() == 2)
    {
        m_nodeIp = results[0];
        m_nodePort = std::stoi(results[1]);
    }
}

void FSN_ServantBase::AddFsnAccount(boost::shared_ptr<FSN_Data> fsn)
{
    boost::lock_guard<boost::recursive_mutex> lock(All_FSN_Guard);
    All_FSN.push_back(fsn);
}

bool FSN_ServantBase::RemoveFsnAccount(boost::shared_ptr<FSN_Data> fsn)
{
    boost::lock_guard<boost::recursive_mutex> lock(All_FSN_Guard);
    const auto & it = std::find_if(All_FSN.begin(), All_FSN.end(),
                                   [fsn] (const boost::shared_ptr<FSN_Data> &other) {
        return *fsn == *other;
    });

    if (it==All_FSN.end()) return false;
    All_FSN.erase(it);
    return true;
}

}
