#include "PosProxy.h"

void supernode::PosProxy::Init() {
	m_DAPIServer->ADD_DAPI_HANDLER(Sale, rpc_command::POS_SALE, PosProxy);
	// TODO: add all other handlers
}


bool supernode::PosProxy::Sale(const rpc_command::POS_SALE::request& in, rpc_command::POS_SALE::response& out) {
	boost::shared_ptr<PosSaleObject> data = boost::shared_ptr<PosSaleObject>( new PosSaleObject() );
	Setup(data);
	if( !data->Init(in) ) return false;
	Add(data);

	out.BlockNum = data->TransactionRecord.BlockNum;
	out.PaymentID = data->TransactionRecord.PaymentID;

	return true;
}
