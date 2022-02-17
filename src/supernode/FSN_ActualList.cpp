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

#include "FSN_ActualList.h"
#include "P2P_Broadcast.h"
#include "DAPI_RPC_Server.h"
#include "DAPI_RPC_Client.h"
#include <unistd.h>

static const unsigned s_AuditTime = 50*60*1000;//50 min
static const uint64_t s_MinStakeBalance = 0;

namespace supernode {

FSN_ActualList::FSN_ActualList(FSN_ServantBase* servant, P2P_Broadcast* p2p, DAPI_RPC_Server* dapi) : m_All_FSN_Guard(servant->All_FSN_Guard), m_All_FSN(servant->All_FSN) {
	m_Servant = servant;
	m_P2P = p2p;
	m_DAPIServer = dapi;


	m_P2P->AddHandler<rpc_command::BROADCACT_ADD_FULL_SUPER_NODE>( p2p_call::AddFSN, bind(&FSN_ActualList::OnAddFSN, this, boost::placeholders::_1) );
	m_P2P->AddHandler<rpc_command::BROADCACT_LOST_STATUS_FULL_SUPER_NODE>( p2p_call::LostFSNStatus, bind(&FSN_ActualList::OnLostFSNStatus, this, boost::placeholders::_1) );
	m_DAPIServer->ADD_DAPI_HANDLER(FSN_CheckWalletOwnership, rpc_command::FSN_CHECK_WALLET_OWNERSHIP, FSN_ActualList);
	m_P2P->AddNearHandler<rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::request, rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::response>( p2p_call::GetFSNList, bind(&FSN_ActualList::GetFSNList, this, boost::placeholders::_1, boost::placeholders::_2) );
}

void FSN_ActualList::Start() {
	CheckIfIamFSN(true);// may be very slow operation

	m_Work.Workers(10);

	m_Running = true;
	m_Thread = new boost::thread(&FSN_ActualList::Run, this);
}

void FSN_ActualList::Stop() {
	m_Running = false;
	m_Thread->join();
	m_Work.Stop();
}

void FSN_ActualList::GetFSNList(const rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::request& in, rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::response& out) {
	boost::lock_guard<boost::recursive_mutex> lock(m_All_FSN_Guard);
	for(auto a : m_All_FSN) {
		rpc_command::BROADCACT_ADD_FULL_SUPER_NODE data;
		data.IP = a->IP;
		data.Port = a->Port;
		data.StakeAddr = a->Stake.Addr;
		data.StakeViewKey = a->Stake.ViewKey;
		data.MinerAddr = a->Miner.Addr;
		data.MinerAddr = a->Miner.ViewKey;
		out.List.push_back(data);
	}


}


void FSN_ActualList::Run() {

	{
		rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::request in;
		vector<rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::response> outv;
		m_P2P->SendNear(p2p_call::GetFSNList, in, outv);

		boost::lock_guard<boost::recursive_mutex> lock(m_All_FSN_Guard);
		for(auto aa : outv)
			for(auto a : aa.List) _OnAddFSN(a);

	}



	while(m_Running) {
		CheckIfIamFSN();

		while(m_Running) {
			auto now = boost::posix_time::second_clock::local_time();
			if( (now-m_AuditStartAt).total_milliseconds()>s_AuditTime ) break;
			sleep(1);
		}
		if(!m_Running) break;

		m_AuditStartAt = boost::posix_time::second_clock::local_time();
		DoAudit();
	}


}

void FSN_ActualList::DoAudit() {
	vector< boost::shared_ptr<FSN_Data> > all;
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_All_FSN_Guard);
		all = m_All_FSN;
	}

	for(unsigned i=0;i<all.size() && m_Running;i++) {
		if( CheckIsFSN(all[i]) ) continue;
		auto data = m_Servant->FSN_DataByStakeAddr( all[i]->Stake.Addr );
		if( !data ) continue;// was deleted

		rpc_command::BROADCACT_LOST_STATUS_FULL_SUPER_NODE in;
		in.StakeAddr = data->Stake.Addr;
		m_P2P->Send(p2p_call::LostFSNStatus, in);
		m_Servant->RemoveFsnAccount(data);
	}//for

}

void FSN_ActualList::CheckIfIamFSN(bool checkOnly) {
	boost::shared_ptr<FSN_Data> data = boost::shared_ptr<FSN_Data>( new FSN_Data(m_Servant->GetMyStakeWallet(), m_Servant->GetMyMinerWallet(), m_DAPIServer->IP(), m_DAPIServer->Port()) );
	if( !CheckIsFSN(data) ) return;

	if(checkOnly) return;

	{
		boost::lock_guard<boost::recursive_mutex> lock(m_All_FSN_Guard);
		for(auto a : m_All_FSN) if( a->IP==data->IP && a->Port==data->Port ) return;
		m_Servant->AddFsnAccount(data);
	}

	rpc_command::BROADCACT_ADD_FULL_SUPER_NODE in;
	in.IP = data->IP;
	in.Port = data->Port;
	in.StakeAddr = data->Stake.Addr;
	in.StakeViewKey = data->Stake.ViewKey;
	in.MinerAddr = data->Miner.Addr;
	in.MinerViewKey = data->Miner.ViewKey;

	m_P2P->Send(p2p_call::AddFSN, in);
}

boost::shared_ptr<FSN_Data> FSN_ActualList::_OnAddFSN(const rpc_command::BROADCACT_ADD_FULL_SUPER_NODE& in ) {
	for(auto a : m_All_FSN) if( a->IP==in.IP && a->Port==in.Port ) return nullptr;

	boost::shared_ptr<FSN_Data> data = boost::shared_ptr<FSN_Data>( new FSN_Data( FSN_WalletData(in.StakeAddr, in.StakeViewKey), FSN_WalletData(in.MinerAddr, in.MinerViewKey), in.IP, in.Port) );

	return data;
}

void FSN_ActualList::OnAddFSN(const rpc_command::BROADCACT_ADD_FULL_SUPER_NODE& in ) {
	m_Work.Service.post( [this, in](){
		OnAddFSNFromWorker(in);
	} );
}

void FSN_ActualList::OnAddFSNFromWorker(const rpc_command::BROADCACT_ADD_FULL_SUPER_NODE& in ) {
	boost::shared_ptr<FSN_Data> data;
	{
		boost::lock_guard<boost::recursive_mutex> lock(m_All_FSN_Guard);
		data = _OnAddFSN(in);
	}

	if( data && CheckIsFSN(data) ) m_Servant->AddFsnAccount(data);// VERY SLOW!!!
}

void FSN_ActualList::OnLostFSNStatus(const rpc_command::BROADCACT_LOST_STATUS_FULL_SUPER_NODE& in) {
	m_Work.Service.post( [this, in](){
		OnLostFSNStatusFromWorker(in);
	} );
}

void FSN_ActualList::OnLostFSNStatusFromWorker(const rpc_command::BROADCACT_LOST_STATUS_FULL_SUPER_NODE& in) {
	boost::shared_ptr<FSN_Data> data = m_Servant->FSN_DataByStakeAddr(in.StakeAddr);
	if(!data) return;
	if( CheckIsFSN(data) ) return;
	m_Servant->RemoveFsnAccount(data);
}

bool FSN_ActualList::CheckWalletOwner(boost::shared_ptr<FSN_Data> data, const string& wa) {
	rpc_command::FSN_CHECK_WALLET_OWNERSHIP::request in;
	rpc_command::FSN_CHECK_WALLET_OWNERSHIP::response out;
	in.Str = GenStrForSign( data->IP, data->Port, wa );
	in.WalletAddr = wa;

	DAPI_RPC_Client call;
	call.Set(data->IP, data->Port);
	if( !call.Invoke(dapi_call::FSN_CheckWalletOwnership, in, out) ) return false;
	return m_Servant->IsSignValid(in.Str, in.WalletAddr, out.Sign);

}

bool FSN_ActualList::CheckIsFSN(boost::shared_ptr<FSN_Data> data) {
	if( !CheckWalletOwner(data, data->Stake.Addr) ) return false;
	if( !CheckWalletOwner(data, data->Miner.Addr) ) return false;


	uint64_t bal = m_Servant->GetWalletBalance( m_Servant->GetCurrentBlockHeight(), data->Stake );
    return bal > s_MinStakeBalance;
}

bool FSN_ActualList::FSN_CheckWalletOwnership(const rpc_command::FSN_CHECK_WALLET_OWNERSHIP::request& in, rpc_command::FSN_CHECK_WALLET_OWNERSHIP::response& out) {
	out.Sign = m_Servant->SignByWalletPrivateKey(in.Str, in.WalletAddr);
	return true;
}

string FSN_ActualList::GenStrForSign(const string& dapiIP, const string& dapiPort, const string& walletAddr) {
	return dapiIP + string(":") + dapiPort + string(":") + walletAddr;
}






}
