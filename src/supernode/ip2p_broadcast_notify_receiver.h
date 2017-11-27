#ifndef ip2p_broadcast_notify_receiver_H_H_H_
#define ip2p_broadcast_notify_receiver_H_H_H_


#include "real_time_commands.h"



class ip2p_broadcast_notify_receiver {
	public:// multi-thread calls. may block for big time, up to 30 secs
	virtual void on_add_full_super_node(const real_time_rpc::BROADCACT_ADD_FULL_SUPER_NODE::request& req)=0;
	virtual void on_del_full_super_node(const real_time_rpc::BROADCACT_DEL_FULL_SUPER_NODE::request& req)=0;
	virtual void on_lost_status_full_super_node(const real_time_rpc::BROADCACT_LOST_STATUS_FULL_SUPER_NODE::request& req)=0;
};




#endif
