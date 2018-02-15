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

#ifndef FSN_ACTUALLIST_H_
#define FSN_ACTUALLIST_H_

#include "FSN_ServantBase.h"
#include "supernode_rpc_command.h"
#include "net/jsonrpc_structs.h"
#include "WorkerPool.h"

namespace supernode {

class P2P_Broadcast;
class DAPI_RPC_Server;


class FSN_ActualList {
public:
	FSN_ActualList(FSN_ServantBase* servant, P2P_Broadcast* p2p, DAPI_RPC_Server* dapi);
	void Start();
	void Stop();

public:
	void OnAddFSN(const rpc_command::BROADCACT_ADD_FULL_SUPER_NODE& in );
	void OnLostFSNStatus(const rpc_command::BROADCACT_LOST_STATUS_FULL_SUPER_NODE& in);
    bool FSN_CheckWalletOwnership(const rpc_command::FSN_CHECK_WALLET_OWNERSHIP::request& in, rpc_command::FSN_CHECK_WALLET_OWNERSHIP::response& out, epee::json_rpc::error &er);
    void GetFSNList(const rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::request& in, rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::response& out, epee::json_rpc::error &er);

	void OnAddFSNFromWorker(const rpc_command::BROADCACT_ADD_FULL_SUPER_NODE& in );
	void OnLostFSNStatusFromWorker(const rpc_command::BROADCACT_LOST_STATUS_FULL_SUPER_NODE& in);

protected:
	string GenStrForSign(const string& dapiIP, const string& dapiPort, const string& walletAddr);
	bool CheckIsFSN(boost::shared_ptr<FSN_Data> data);
	bool CheckWalletOwner(boost::shared_ptr<FSN_Data> data, const string& wa);
	boost::shared_ptr<FSN_Data> _OnAddFSN(const rpc_command::BROADCACT_ADD_FULL_SUPER_NODE& in );
	void Run();
	void CheckIfIamFSN(bool checkOnly=false);
	virtual void DoAudit();

protected:
    boost::recursive_mutex& m_All_FSN_Guard;// DO NOT block for long time. if need - use copy
    vector< boost::shared_ptr<FSN_Data> >& m_All_FSN;// access to this data may be done from different threads
    P2P_Broadcast* m_P2P = nullptr;
    DAPI_RPC_Server* m_DAPIServer = nullptr;
    FSN_ServantBase* m_Servant = nullptr;
    bool m_Running = false;
    boost::thread* m_Thread = nullptr;

    WorkerPool m_Work;
    boost::posix_time::ptime m_AuditStartAt;

};




}

#endif /* FSN_ACTUALLIST_H_ */
