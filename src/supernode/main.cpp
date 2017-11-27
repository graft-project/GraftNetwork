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

#include "supernode_rpc_server.h"
#include "real_time_auth.h"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/tokenizer.hpp>
#include "misc_log_ex.h"




int main(int argc, char** argv) {

	// this not working - log still filled
	//el::Loggers::setCategories("", true);
	//mlog_set_categories("");
	//mlog_set_log_level(32);


	string conf_file("aconf.ini");
	if(argc>1) conf_file = argv[1];
	LOG_PRINT_L4("conf: "<<conf_file);

	// load config
	boost::property_tree::ptree config;
	boost::property_tree::ini_parser::read_ini(conf_file, config);


	// real time auth will be started in separated thread
    real_time_auth rta;
    ip2p_boradcast_sender broad_cast_sender;
    broad_cast_sender.reciver = &rta;
    broad_cast_sender.set_conf(config);
    boost::thread p2p_thread( boost::bind(&ip2p_boradcast_sender::run_thread, &broad_cast_sender) );

    rta.init_and_start( &broad_cast_sender, config);

    /*
    {// for test only
    	string ipp = config.get<string>("seed_list.seed");

    	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    	boost::char_separator<char> sep(":");
    	tokenizer tokens(ipp, sep);

    	real_time_rpc::BROADCACT_ADD_FULL_SUPER_NODE::request rr;
    	tokenizer::iterator tok_iter = tokens.begin();
    	rr.ip = *tok_iter;
    	tok_iter++;
    	rr.port = *tok_iter;

    	boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

    	rta.on_add_full_super_node(rr);
    }
*/

    // configure and run dapi in main thread
    const boost::property_tree::ptree& dapi_conf = config.get_child("dapi");

    tools::supernode_rpc_server rpc;
    rpc.init( dapi_conf.get<string>("port"), dapi_conf.get<string>("ip") );
	rpc.run( dapi_conf.get<int>("threads", 5) );// block execution


	rta.stop();

	return 0;
}



