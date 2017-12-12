#include "BaseRTAProcessor.h"


static const unsigned s_ObjectLifetime = 20*60*1000;//20 min

supernode::BaseRTAProcessor::~BaseRTAProcessor() {}

void supernode::BaseRTAProcessor::Start() {}
void supernode::BaseRTAProcessor::Stop() {}

void supernode::BaseRTAProcessor::Set(const FSN_ServantBase* ser, DAPI_RPC_Server* dapi) {
	m_Servant = ser;
	m_DAPIServer = dapi;
	Init();
}

void supernode::BaseRTAProcessor::Add(boost::shared_ptr<BaseRTAObject> obj) {
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_ObjectsGuard);
		m_Objects.push_back(obj);
	}
	Tick();
}

void supernode::BaseRTAProcessor::Setup(boost::shared_ptr<BaseRTAObject> obj) {
	obj->Set(m_Servant, m_DAPIServer);
}

boost::shared_ptr<supernode::BaseRTAObject> supernode::BaseRTAProcessor::ObjectByPayment(const string& payment_id) {
	boost::shared_ptr<BaseRTAObject> ret;
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_ObjectsGuard);
		for(auto& a : m_Objects) if(a->TransactionRecord.PaymentID==payment_id) {
			ret = a;
			break;
		}
	}
	return ret;
}

void supernode::BaseRTAProcessor::Remove(boost::shared_ptr<BaseRTAObject> obj) {
	obj->MarkForDelete();
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_ObjectsGuard);
		auto it = find(m_Objects.begin(), m_Objects.end(), obj);
		if( it!=m_Objects.end() ) m_Objects.erase(it);

	}
	{
		obj->TimeMark = boost::posix_time::second_clock::local_time();
		boost::lock_guard<boost::recursive_mutex> lock(m_RemoveObjectsGuard);
		m_RemoveObjects.push_back( obj );
	}

}


void supernode::BaseRTAProcessor::Tick() {
	auto now = boost::posix_time::second_clock::local_time();
	{
		vector< boost::shared_ptr<BaseRTAObject> > vv;
		{
			boost::lock_guard<boost::recursive_mutex> lock(m_ObjectsGuard);
			for(auto a: m_Objects) if( (now-a->TimeMark).total_milliseconds()>s_ObjectLifetime ) vv.push_back(a);//20 min
		}
		for(auto a : vv) Remove(a);
	}
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_RemoveObjectsGuard);
		for(int i=0;i<int(m_RemoveObjects.size());i++) if( (now-m_RemoveObjects[i]->TimeMark).total_milliseconds()>(5*60*1000) ) {
			m_RemoveObjects.erase( m_RemoveObjects.begin()+i );
			//LOG_PRINT_L5("---- ERASE ----");
			i--;
		}
	}

}




