#ifndef REALTIMEAUTH_H_H_H_
#define REALTIMEAUTH_H_H_H_


#include <string>
#include <vector>
#include "net/http_server_impl_base.h"
#include "storages/http_abstract_invoke.h"
#include "real_time_commands.h"
#include <boost/thread/mutex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/thread.hpp>
#include "http_rpc_client.h"
#include "ip2p_boradcast_sender.h"
#include "ip2p_broadcast_notify_receiver.h"
using namespace std;

// how to this work
// 1. check, if we reach full super node status, send broad cast about this
// 2. check, if full super node status lost, send broad cast about this
// 3. if we're full super node, time to time check other full super nodes from list, is them have full super node status
// 3.1. if some full super node lost there full supernode status, send broadcast
// 4. if we got broad cast, that some supernode reach full supernode status, check first, if ok, add to list
// 5. for check we have PoS and PoW functions impl
// 6. When node rise up, it need send broadcast message to collect all other full-supernode IP
//
// for simplify, rpc server and real time auth logic will be integrated in one class - we have too small rpc calls and too small RTA logic
// to do separated classes. if logic will be grow up, we must refactory code and create separeted classes






class real_time_auth : public epee::http_server_impl_base<real_time_auth>, public ip2p_broadcast_notify_receiver {
	public:
	typedef epee::net_utils::connection_context_base connection_context;

	enum class audit_state {
		none,// just add, not checked yet
		need_audit,// schedule for audit, must process by first free worker
		in_audit,//
		audit_done,// audit done
	};

	struct full_supernode_data : public real_time_rpc::full_super_node_data {
		audit_state status = audit_state::none;
	};

	public:
	virtual ~real_time_auth();
	void init_and_start(ip2p_boradcast_sender* sender, const boost::property_tree::ptree& conf);
	void stop();

	public:
	template<class T>
	void send_broadcast_message(int cmd, const T& mes) {
		boost::lock_guard<boost::mutex> lock(m_broadcast_send_guard);
		m_broadcast_sender->send_broadcast_message(cmd, mes);
	}

	protected:
    CHAIN_HTTP_TO_MAP2(connection_context) //forward http requests to uri map

	BEGIN_URI_MAP2()
		BEGIN_JSON_RPC_MAP("/rpc")
            MAP_JON_RPC_WE("stake_owner_ship",         on_stake_owner_ship,       real_time_rpc::COMMAND_CHECK_STAKE_OWNERSHIP)
            MAP_JON_RPC_WE("miner_owner_ship",         on_miner_owner_ship,       real_time_rpc::COMMAND_CHECK_MINER_OWNERSHIP)
		END_JSON_RPC_MAP()
	END_URI_MAP2()

	protected://json_rpc
    bool on_stake_owner_ship(const real_time_rpc::COMMAND_CHECK_STAKE_OWNERSHIP::request& req, real_time_rpc::COMMAND_CHECK_STAKE_OWNERSHIP::response& res, epee::json_rpc::error& er);
    bool on_miner_owner_ship(const real_time_rpc::COMMAND_CHECK_MINER_OWNERSHIP::request& req, real_time_rpc::COMMAND_CHECK_MINER_OWNERSHIP::response& res, epee::json_rpc::error& er);


	public:// broad_cast handlers
	void on_add_full_super_node(const real_time_rpc::BROADCACT_ADD_FULL_SUPER_NODE::request& req);
	void on_del_full_super_node(const real_time_rpc::BROADCACT_DEL_FULL_SUPER_NODE::request& req);
	void on_lost_status_full_super_node(const real_time_rpc::BROADCACT_LOST_STATUS_FULL_SUPER_NODE::request& req);

	protected:
	bool check_is_full_supernode(boost::shared_ptr<full_supernode_data> data);
	void remove_and_notify( boost::shared_ptr<full_supernode_data> data, bool notify=true );
	void check_is_full_supernode();

	protected:
	void rpc_thread_run();
	void audit_thread_run();
	void audit_worker_run();

	protected:
	struct wallet_data {
		string addr;
		string sign;//cache for checks
	};

	void fill_from_conf(wallet_data& data, const boost::property_tree::ptree& conf);

	protected:
	boost::mutex m_fsn_data_guard;
	vector< boost::shared_ptr<full_supernode_data> > m_fsn_data;
	bool m_audit_in_process = false;
	boost::thread_group m_audit_workers;
	int m_audit_time_delta_secs = 60;

	protected:
	boost::mutex m_broadcast_send_guard;
	ip2p_boradcast_sender* m_broadcast_sender = nullptr;

	protected:
	bool m_running = false;
	bool m_is_full_supernode = false;
	int m_rpc_threads_count = 5;
	boost::thread* m_rpc_thread = nullptr;
	boost::thread* m_audit_thread = nullptr;

	protected:
	wallet_data m_stake_wallet;
	wallet_data m_miner_wallet;
	full_supernode_data m_full_supernode_data;

};
















#endif
