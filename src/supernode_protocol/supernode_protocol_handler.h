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

#ifndef GRAFTNETWORK_SUPERNODE_PROTOCOL_HANDLER_H
#define GRAFTNETWORK_SUPERNODE_PROTOCOL_HANDLER_H

#include <supernode_basic/connection_context.h>
#include "supernode_protocol_defs.h"

namespace supernode {
    template <class t_core>
    class protocol_handler {
    public:
        typedef supernode::connection_context connection_context;
    private:
        typedef std::string blobdata;

        nodetool::p2p_endpoint_stub<connection_context> m_p2p_stub;
        nodetool::i_p2p_endpoint<connection_context>* m_p2p;

        epee::math_helper::once_a_time_seconds<30> m_idle_peer_kicker;
    public:
        typedef supernode::protocol::core_stat_info stat_info;
        typedef supernode::protocol::CORE_SYNC_DATA payload_type;

        BEGIN_INVOKE_MAP2(protocol_handler)

        END_INVOKE_MAP2()

        bool process_payload_sync_data(const supernode::protocol::CORE_SYNC_DATA& hshd, connection_context& context, bool is_inital);
        bool get_payload_sync_data(supernode::protocol::CORE_SYNC_DATA& hshd);
        bool get_stat_info(supernode::protocol::core_stat_info& stat_inf);
        void set_p2p_endpoint(nodetool::i_p2p_endpoint<connection_context>* p2p);
        bool on_callback(connection_context& context);
        void on_connection_close(connection_context &context);
        bool on_idle();
        bool kick_idle_peers();
    };
}

#include "supernode_protocol_handler.inl"

#endif //GRAFTNETWORK_SUPERNODE_PROTOCOL_HANDLER_H
