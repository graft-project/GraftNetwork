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
//

#include <supernode/P2P_Broadcast.h>


namespace supernode {

void P2P_Broadcast::Set(DAPI_RPC_Server* pa, const vector<string>& trustedRing) {
	m_DAPIServer = pa;
	m_SubNet.Set(pa, "p2p", trustedRing);
	m_SubNet.RetryCount = 1;
	m_SubNet.CallTimeout = std::chrono::seconds(1);
	m_SubNet.AllowSendSefl = false;

	AddHandler<rpc_command::P2P_ADD_NODE_TO_LIST>( p2p_call::AddSeed, bind(&P2P_Broadcast::AddSeed, this, _1) );
	AddNearHandler<rpc_command::P2P_GET_ALL_NODES_LIST::request, rpc_command::P2P_GET_ALL_NODES_LIST::response>( p2p_call::GetSeedsList, bind(&P2P_Broadcast::GetSeedsList, this, _1, _2) );
}

vector< pair<string, string> > P2P_Broadcast::Seeds() { return m_SubNet.Members(); }

void P2P_Broadcast::Start() {
	rpc_command::P2P_GET_ALL_NODES_LIST::request in;
	vector<rpc_command::P2P_GET_ALL_NODES_LIST::response> out;
	SendNear(p2p_call::GetSeedsList,  in, out);
	//if(out.empty()) throw string("no seeds - can't start P2P");

	for(auto& a : out)
		for(auto& aa : a.List) AddSeed(aa);

	rpc_command::P2P_ADD_NODE_TO_LIST add;
	add.IP = m_DAPIServer->IP();
	add.Port = m_DAPIServer->Port();

	Send(p2p_call::AddSeed, add);

}


void P2P_Broadcast::Stop() {}


void P2P_Broadcast::AddSeed(const rpc_command::P2P_ADD_NODE_TO_LIST& in ) {
	m_SubNet.AddMember( in.IP, in.Port );
}

void P2P_Broadcast::GetSeedsList(const rpc_command::P2P_GET_ALL_NODES_LIST::request& in, rpc_command::P2P_GET_ALL_NODES_LIST::response& out) {
	vector< pair<string, string> > vv = m_SubNet.Members();
	for(auto& a : vv) {
		rpc_command::P2P_ADD_NODE_TO_LIST t;
		t.IP = a.first;
		t.Port= a.second;
		out.List.push_back( t );
	}
}


};
