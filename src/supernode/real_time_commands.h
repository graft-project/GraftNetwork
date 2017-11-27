#ifndef REALTIMECOMMANDS_H__H_H
#define REALTIMECOMMANDS_H__H_H

#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "crypto/hash.h"
#include <string>
using namespace std;

namespace real_time_rpc {

	struct COMMAND_CHECK_STAKE_OWNERSHIP {
		struct request {
			BEGIN_KV_SERIALIZE_MAP()
			END_KV_SERIALIZE_MAP()
		};

		struct response {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(sign)
			END_KV_SERIALIZE_MAP()

			string sign;// sign ip:port+stake wallet addr by wallet private key
		};
	};

	struct COMMAND_CHECK_MINER_OWNERSHIP {
		struct request {
			BEGIN_KV_SERIALIZE_MAP()
			END_KV_SERIALIZE_MAP()
		};

		struct response {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(sign)
			END_KV_SERIALIZE_MAP()

			string sign;// sign ip:port+mining wallet addr by wallet private key
		};
	};



	// ------------------ broadcasts -------------------------

	enum broadcast_commands {
		broadcast_add_fsn = 1,
		broadcast_del_fsn = 2,
		broadcast_lost_fsn_status = 3
	};

	struct full_super_node_data {
		string stake_wallet_addr;
		string miner_wallet_addr;
		string ip;
		string port;
	};


	struct BROADCACT_ADD_FULL_SUPER_NODE {

		struct request : public full_super_node_data {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(stake_wallet_addr)
				KV_SERIALIZE(miner_wallet_addr)
				KV_SERIALIZE(ip)
				KV_SERIALIZE(port)
			END_KV_SERIALIZE_MAP()

		};

		struct response {
			BEGIN_KV_SERIALIZE_MAP()
			END_KV_SERIALIZE_MAP()
		};

	};

	struct BROADCACT_DEL_FULL_SUPER_NODE {

		struct request {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(ip)
				KV_SERIALIZE(port)
				KV_SERIALIZE(random_string)
				KV_SERIALIZE(sign)
			END_KV_SERIALIZE_MAP()

			string ip;
			string port;
			string random_string;
			string sign;//ip:port+random_string signed by stake wallet sign. so we can check, if this message realy sended by owner of this node
		};

		struct response {
			BEGIN_KV_SERIALIZE_MAP()
			END_KV_SERIALIZE_MAP()
		};

	};

	struct BROADCACT_LOST_STATUS_FULL_SUPER_NODE {

		struct request {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(ip)
				KV_SERIALIZE(port)
			END_KV_SERIALIZE_MAP()

			string ip;
			string port;
		};

		struct response {
			BEGIN_KV_SERIALIZE_MAP()
			END_KV_SERIALIZE_MAP()
		};

	};



};

















#endif
