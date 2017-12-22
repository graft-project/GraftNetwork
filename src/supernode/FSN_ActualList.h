#ifndef FSN_ACTUALLIST_H_
#define FSN_ACTUALLIST_H_

#include "FSN_ServantBase.h"
#include "supernode_rpc_command.h"


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
	bool FSN_CheckWalletOwnership(const rpc_command::FSN_CHECK_WALLET_OWNERSHIP::request& in, rpc_command::FSN_CHECK_WALLET_OWNERSHIP::response& out);
	void GetFSNList(const rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::request& in, rpc_command::BROADCAST_NEAR_GET_ACTUAL_FSN_LIST::response& out);

protected:
	string GenStrForSign(const string& dapiIP, const string& dapiPort, const string& walletAddr);
	bool CheckIsFSN(boost::shared_ptr<FSN_Data> data);
	bool CheckWalletOwner(boost::shared_ptr<FSN_Data> data, const string& wa);
	boost::shared_ptr<FSN_Data> _OnAddFSN(const rpc_command::BROADCACT_ADD_FULL_SUPER_NODE& in );
	void Run();
	void CheckIfIamFSN();

protected:
    boost::recursive_mutex& m_All_FSN_Guard;// DO NOT block for long time. if need - use copy
    vector< boost::shared_ptr<FSN_Data> >& m_All_FSN;// access to this data may be done from different threads
    P2P_Broadcast* m_P2P = nullptr;
    DAPI_RPC_Server* m_DAPIServer = nullptr;
    FSN_ServantBase* m_Servant = nullptr;
    bool m_Running = false;
    boost::thread* m_Thread = nullptr;

};




}

#endif /* FSN_ACTUALLIST_H_ */
