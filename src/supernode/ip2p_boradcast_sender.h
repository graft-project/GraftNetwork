#ifndef IP2PBORADCASTSENDER_H_
#define IP2PBORADCASTSENDER_H_

#include <string>
#include <vector>
#include "net/http_server_impl_base.h"
#include "storages/http_abstract_invoke.h"
#include "real_time_commands.h"
#include <boost/thread/mutex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/thread.hpp>
#include "http_rpc_client.h"
#include "ip2p_broadcast_notify_receiver.h"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/tokenizer.hpp>


class ip2p_boradcast_sender : public epee::http_server_impl_base<ip2p_boradcast_sender> {
	public:
	typedef epee::net_utils::connection_context_base connection_context;

	string my_port;
	string my_ip;

	string other_port;
	string other_ip;

	void set_conf(boost::property_tree::ptree& in_conf) {
		const boost::property_tree::ptree& conf = in_conf.get_child("broadcast");
		my_port = conf.get<string>("port");
		my_ip = conf.get<string>("ip");

    	string ipp = conf.get<string>("seeds");
    	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    	boost::char_separator<char> sep(":");
    	tokenizer tokens(ipp, sep);

    	tokenizer::iterator tok_iter = tokens.begin();
    	other_ip = *tok_iter;
    	tok_iter++;
    	other_port = *tok_iter;
	}

	void run_thread() {
		init(my_port, my_ip);
		run(2);
	}


    CHAIN_HTTP_TO_MAP2(connection_context) //forward http requests to uri map

	BEGIN_URI_MAP2()
		BEGIN_JSON_RPC_MAP("/rpc_broadcast")
            MAP_JON_RPC_WE("1",         on_one,       real_time_rpc::BROADCACT_ADD_FULL_SUPER_NODE)
            MAP_JON_RPC_WE("2",         on_two,       real_time_rpc::BROADCACT_DEL_FULL_SUPER_NODE)
            MAP_JON_RPC_WE("3",         on_three,       real_time_rpc::BROADCACT_LOST_STATUS_FULL_SUPER_NODE)
		END_JSON_RPC_MAP()
	END_URI_MAP2()

	struct dummy_response {
		BEGIN_KV_SERIALIZE_MAP()
		END_KV_SERIALIZE_MAP()
	};


	template<class T>
	void send_broadcast_message(int command, const T& mes) {
    	http_rpc_client call;
    	call.set(other_ip, other_port, "http", "/rpc_broadcast");

    	string cmd = boost::lexical_cast<std::string>(command);

    	LOG_PRINT_L4("\n\n\n MY: "<<my_ip<<":"<<my_port<<"  OTHER: "<<other_ip<<":"<<other_port<<"  cmd_in: "<<command<<"  txt: '"<<cmd<<"'"<<"\n\n\n");

    	dummy_response resp;
    	call.invoke_http_json(cmd, mes, resp);
	}

	ip2p_broadcast_notify_receiver* reciver = nullptr;

	bool on_one(const real_time_rpc::BROADCACT_ADD_FULL_SUPER_NODE::request& req, real_time_rpc::BROADCACT_ADD_FULL_SUPER_NODE::response& res, epee::json_rpc::error& er) {
		reciver->on_add_full_super_node(req);
		return true;
	}

	bool on_two(const real_time_rpc::BROADCACT_DEL_FULL_SUPER_NODE::request& req, real_time_rpc::BROADCACT_DEL_FULL_SUPER_NODE::response& res, epee::json_rpc::error& er) {
		reciver->on_del_full_super_node(req);
		return true;
	}

	bool on_three(const real_time_rpc::BROADCACT_LOST_STATUS_FULL_SUPER_NODE::request& req, real_time_rpc::BROADCACT_LOST_STATUS_FULL_SUPER_NODE::response& res, epee::json_rpc::error& er) {
		reciver->on_lost_status_full_super_node(req);
		return true;
	}

};


#endif /* IP2PBORADCASTSENDER_H_ */
