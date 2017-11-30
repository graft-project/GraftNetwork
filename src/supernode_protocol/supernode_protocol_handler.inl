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
// Parts of this file are originally copyright (c) 2014-2017 The Monero Project

#include "supernode_protocol_handler.h"

#define IDLE_PEER_KICK_TIME (45 * 1000000) // microseconds

template<class t_core>
bool supernode::protocol_handler<t_core>::process_payload_sync_data(const supernode::protocol::CORE_SYNC_DATA &hshd, protocol_handler::connection_context &context, bool is_inital) {
    return true;
}

template<class t_core>
bool supernode::protocol_handler<t_core>::get_payload_sync_data(supernode::protocol::CORE_SYNC_DATA &hshd) {
    return false;
}

template<class t_core>
bool supernode::protocol_handler<t_core>::get_stat_info(supernode::protocol::core_stat_info &stat_inf) {
    return false;
}

template<class t_core>
void supernode::protocol_handler<t_core>::set_p2p_endpoint(nodetool::i_p2p_endpoint<connection_context>* p2p)
{
    if(p2p) {
        m_p2p = p2p;
    } else {
        m_p2p = &m_p2p_stub;
    }
}

template<class t_core>
bool supernode::protocol_handler<t_core>::on_callback(protocol_handler::connection_context &context) {
    return false;
}

template<class t_core>
void supernode::protocol_handler<t_core>::on_connection_close(protocol_handler::connection_context &context) {

}

template<class t_core>
bool supernode::protocol_handler<t_core>::on_idle() {
    m_idle_peer_kicker.do_call(boost::bind(&protocol_handler<t_core>::kick_idle_peers, this));
    return true;
}

template<class t_core>
bool supernode::protocol_handler<t_core>::kick_idle_peers() {
    MTRACE("Checking for idle peers...");
    m_p2p->for_each_connection([&](connection_context& context, nodetool::peerid_type peer_id, uint32_t support_flags)->bool
    {
       if (context.m_state == connection_context::state_synchronizing)
       {
           const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
           const boost::posix_time::time_duration dt = now - context.m_last_request_time;
           if (dt.total_microseconds() > IDLE_PEER_KICK_TIME)
           {
               MINFO(context << " kicking idle peer");
               ++context.m_callback_request_count;
               m_p2p->request_callback(context);
           }
       }
       return true;
    });
    return true;
}
