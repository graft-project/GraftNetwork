#include "real_time_auth.h"
#include <boost/thread.hpp>


using namespace epee;

real_time_auth::~real_time_auth() {
	if(m_rpc_thread) {
		delete m_rpc_thread;
		m_rpc_thread = nullptr;
	}

	if(m_audit_thread) {
		delete m_audit_thread;
		m_audit_thread = nullptr;
	}


}

void real_time_auth::init_and_start(ip2p_boradcast_sender* sender, const boost::property_tree::ptree& in_conf) {
	m_broadcast_sender = sender;


	// init config
	fill_from_conf( m_stake_wallet, in_conf.get_child("stake_wallet") );
	fill_from_conf( m_miner_wallet, in_conf.get_child("miner_wallet") );

	const boost::property_tree::ptree& conf = in_conf.get_child("supernode");
	m_full_supernode_data.ip = conf.get<string>("ip");
	m_full_supernode_data.port = conf.get<string>("port");
	m_full_supernode_data.stake_wallet_addr = m_stake_wallet.addr;
	m_full_supernode_data.miner_wallet_addr = m_miner_wallet.addr;
	m_rpc_threads_count = conf.get<int>("threads", 5);

	// start threads
	m_running = true;

	// create audit workers
	const boost::property_tree::ptree& au_conf = in_conf.get_child("audit");
	m_audit_time_delta_secs = au_conf.get<int>("period");
	int au_t_count = au_conf.get<int>("threads");
	for(int i=0;i<au_t_count;i++) {
		m_audit_workers.create_thread( boost::bind(&real_time_auth::audit_worker_run, this) );
	}

	// main threads
	m_rpc_thread = new boost::thread( boost::bind(&real_time_auth::rpc_thread_run, this) );
	m_audit_thread = new boost::thread( boost::bind(&real_time_auth::audit_thread_run, this) );

}

void real_time_auth::rpc_thread_run() {
	init(m_full_supernode_data.port, m_full_supernode_data.ip);
	run(m_rpc_threads_count);
}

void real_time_auth::stop() {
	m_running = false;
	send_stop_signal();

	if(m_rpc_thread) m_rpc_thread->join();
	if(m_audit_thread) m_audit_thread->join();
	m_audit_workers.join_all();
}


void real_time_auth::audit_thread_run() {
	while(m_running) {
		boost::this_thread::sleep( boost::posix_time::seconds(m_audit_time_delta_secs) );

		check_is_full_supernode();

		// last audit was done, so we now can start next audit
		if(!m_audit_in_process) {
			boost::lock_guard<boost::mutex> lock(m_fsn_data_guard);
			for(auto a : m_fsn_data) if(a->status==audit_state::audit_done) a->status = audit_state::need_audit;
			m_audit_in_process = true;
		}

	}
}

void real_time_auth::check_is_full_supernode() {
	// TODO IMPL
	if(m_is_full_supernode) return;
	m_is_full_supernode = true;
	real_time_rpc::BROADCACT_ADD_FULL_SUPER_NODE::request req;

	real_time_rpc::full_super_node_data& dst = req;
	dst = m_full_supernode_data;

	send_broadcast_message(real_time_rpc::broadcast_add_fsn, req);
}

void real_time_auth::remove_and_notify( boost::shared_ptr<full_supernode_data> data, bool notify ) {
	if(notify) {
		real_time_rpc::BROADCACT_LOST_STATUS_FULL_SUPER_NODE::request req;
		req.ip = data->ip;
		req.port = data->port;
		send_broadcast_message(real_time_rpc::broadcast_lost_fsn_status, req);
	}

	boost::lock_guard<boost::mutex> lock(m_fsn_data_guard);
	auto it = find(m_fsn_data.begin(), m_fsn_data.end(), data);
	if(it!=m_fsn_data.end()) {
		m_fsn_data.erase(it);
	}
}

void real_time_auth::audit_worker_run() {
	while(m_running) {
		while( !m_audit_in_process && m_running ) boost::this_thread::sleep( boost::posix_time::milliseconds(100) );
		if(!m_running) break;

		boost::shared_ptr<full_supernode_data> sd;
		{
			boost::lock_guard<boost::mutex> lock(m_fsn_data_guard);
			for(auto a : m_fsn_data) {
				if( a->status==audit_state::need_audit ) {
					sd = a;
					a->status = audit_state::in_audit;
					break;
				}
			}
		}

		if(!sd) {
			m_audit_in_process = false;
		} else {
			bool r = check_is_full_supernode(sd);
			if(!r) remove_and_notify(sd); else sd->status = audit_state::audit_done;
		}
	}//while
}

void real_time_auth::fill_from_conf(wallet_data& data, const boost::property_tree::ptree& conf) {
	data.addr = conf.get<string>("addr");
	// TODO: create signs
	data.sign = conf.get<string>("sign");
}

bool real_time_auth::on_stake_owner_ship(const real_time_rpc::COMMAND_CHECK_STAKE_OWNERSHIP::request& req, real_time_rpc::COMMAND_CHECK_STAKE_OWNERSHIP::response& res, epee::json_rpc::error& er) {
	res.sign = m_stake_wallet.sign;
	return true;
}

bool real_time_auth::on_miner_owner_ship(const real_time_rpc::COMMAND_CHECK_MINER_OWNERSHIP::request& req, real_time_rpc::COMMAND_CHECK_MINER_OWNERSHIP::response& res, epee::json_rpc::error& er) {
	res.sign = m_miner_wallet.sign;
	return true;
}

bool real_time_auth::check_is_full_supernode(boost::shared_ptr<full_supernode_data> data) {
//	LOG_PRINT_L4("ADD FSN: "<<data->ip<<":"<<data->port);

	http_rpc_client call;
	call.set(data->ip, data->port);

    real_time_rpc::COMMAND_CHECK_STAKE_OWNERSHIP::response resp;
    real_time_rpc::COMMAND_CHECK_STAKE_OWNERSHIP::request req;

	if( !call.invoke_http_json("stake_owner_ship", req, resp) ) {
		return false;
	}

//	LOG_PRINT_L4("\n\nGOT: "<<resp.sign<<"\n\n");

	// after got wallet sign, check for uniq in list on all super_node with status != audit_state::none
	// if duplicate, remove both nodes



	// TODO: IMPL
	return true;
}

void real_time_auth::on_add_full_super_node(const real_time_rpc::BROADCACT_ADD_FULL_SUPER_NODE::request& req) {

//	LOG_PRINT_L4("\n\n OnADD FSN \n\n");

	boost::shared_ptr<full_supernode_data> data;
	{
		boost::lock_guard<boost::mutex> lock(m_fsn_data_guard);
		for(auto a : m_fsn_data) if( a->port==req.port && a->ip==req.ip ) return;

		data = boost::shared_ptr<full_supernode_data>( new full_supernode_data() );

		real_time_rpc::full_super_node_data& dst = *data;
		const real_time_rpc::full_super_node_data& src = req;
		dst = src;


		data->status = audit_state::none;
		m_fsn_data.push_back(data);
	}

	if( check_is_full_supernode(data) ) {
		data->status = audit_state::audit_done;
		return;
	}

	remove_and_notify(data, false);
}


void real_time_auth::on_del_full_super_node(const real_time_rpc::BROADCACT_DEL_FULL_SUPER_NODE::request& req) {
	//TODO: IMPL
}

void real_time_auth::on_lost_status_full_super_node(const real_time_rpc::BROADCACT_LOST_STATUS_FULL_SUPER_NODE::request& req) {
	//TODO: IMPL
}








