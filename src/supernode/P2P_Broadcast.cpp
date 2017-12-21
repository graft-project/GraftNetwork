#include <supernode/P2P_Broadcast.h>

//void supernode::P2P_Broadcast::Set(const string& ip, const string& port, int threadsNum, const vector< pair<string, string> >& seeds ) {}
//void supernode::P2P_Broadcast::Start() {}
//void supernode::P2P_Broadcast::Stop() {}

namespace supernode {

void P2P_Broadcast::Set(DAPI_RPC_Server* pa, const vector<string>& seeds) {
	m_DAPIServer = pa;
	m_SubNet.Set(pa, "p2p", seeds);

}

vector< pair<string, string> > P2P_Broadcast::Seeds() { return m_SubNet.Members(); }

void P2P_Broadcast::Start() {}
void P2P_Broadcast::Stop() {}



};
