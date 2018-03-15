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

#ifndef P2PBROADCAST_H_
#define P2PBROADCAST_H_

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <string>
#include <vector>
#include "SubNetBroadcast.h"

using namespace std;

namespace supernode {

class P2P_Broadcast
{
public:
    // initial setup ip:port for bind for,
    // threadsNum - number of worker thread for send/recv data
    // seeds - neighbors from config, ip, port
    //void Set(const string& ip, const string& port, int threadsNum, const vector< pair<string, string> >& seeds );

    void Set(DAPI_RPC_Server* pa, const vector<string>& trustedRing );

    vector< pair<string, string> > Seeds();

    void Start();// start accept connection, NOT blocked call
    void Stop();//stop all worker threads and wait for it's end (join)

    // if we can;t use method as string and need to use int, so create class enum p2p_command : int {} in
    template<class IN_t>
    void AddHandler( const string& method, boost::function<void (const IN_t&)> handler )
    {
        m_DAPIServer->Add_UUID_MethodHandler<IN_t, rpc_command::P2P_DUMMY_RESP, epee::json_rpc::error>("p2p", method, [handler](const IN_t& in, rpc_command::P2P_DUMMY_RESP& out, epee::json_rpc::error &err)
        {
            handler(in);
            return true;
        }
        );
    }

    template<class IN_t, class OUT_t, class ERR_t>
    void AddNearHandler(const string& method, boost::function<void (const IN_t&, OUT_t&, ERR_t&)> handler)
    {
        m_DAPIServer->Add_UUID_MethodHandler<IN_t, OUT_t, ERR_t>("p2p", method, [handler](const IN_t& in, OUT_t& out, ERR_t &err)
        {
            handler(in, out, err);
            return true;
        });
    }

    template<class IN_t, class OUT_t>
    bool SendNear( const string& method, IN_t& data, vector<OUT_t>& outv ) {
        // TODO: need limit number of sends
        data.PaymentID = "p2p";
        return m_SubNet.Send(method, data, outv, false);
    }

    template<class IN_t>
    void Send( const string& method, IN_t& data )
    {
        data.PaymentID = "p2p";
        return m_SubNet.Send(method, data);
    }

public:
    void AddSeed(const rpc_command::P2P_ADD_NODE_TO_LIST& in );
    void GetSeedsList(const rpc_command::P2P_GET_ALL_NODES_LIST::request& in, rpc_command::P2P_GET_ALL_NODES_LIST::response& out, epee::json_rpc::error &err);

protected:
    SubNetBroadcast m_SubNet;
    DAPI_RPC_Server* m_DAPIServer = nullptr;
};

} /* namespace supernode */

#endif /* P2PBROADCAST_H_ */
