#include "SubNetBroadcast.h"

supernode::SubNetBroadcast::~SubNetBroadcast() {
	for(auto a : m_MyHandlers) m_DAPIServer->RemoveHandler(a);
	m_MyHandlers.clear();
}

void supernode::SubNetBroadcast::Set( DAPI_RPC_Server* pa, string subnet_id, const vector< boost::shared_ptr<FSN_Data> >& members ) {
	m_DAPIServer = pa;
	m_Members = members;
	m_PaymentID = subnet_id;
}
