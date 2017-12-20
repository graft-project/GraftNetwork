#include "SubNetBroadcast.h"
#include "supernode_helpers.h"

supernode::SubNetBroadcast::~SubNetBroadcast() {
	for(auto a : m_MyHandlers) m_DAPIServer->RemoveHandler(a);
	m_MyHandlers.clear();
}

vector< pair<string, string> > supernode::SubNetBroadcast::Members() { return m_Members; }

void supernode::SubNetBroadcast::Set( DAPI_RPC_Server* pa, string subnet_id, const vector< boost::shared_ptr<FSN_Data> >& members ) {
	m_DAPIServer = pa;
	m_PaymentID = subnet_id;
	for(auto a : members) m_Members.push_back( make_pair(a->IP, a->Port) );
}

void supernode::SubNetBroadcast::Set( DAPI_RPC_Server* pa, string subnet_id, const vector<string>& members ) {
	m_DAPIServer = pa;
	m_PaymentID = subnet_id;
	for(auto a : members) {
		vector<string> vv = helpers::StrTok(a, ":");
		if( vv[0]==pa->IP() && vv[1]==pa->Port() ) continue;
		m_Members.push_back( make_pair(vv[0], vv[1]) );
	}
}

