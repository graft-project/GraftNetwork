#ifndef SUPERNODE_RPC_COMMAND_H_
#define SUPERNODE_RPC_COMMAND_H_

#include "supernode_common_struct.h"

namespace supernode {
	namespace rpc_command {
		// ---------------------------------------
		struct SUB_NET_BROADCAST_RESPONCE {
			bool Fail;
		};
		struct RTA_TRANSACTION_OBJECT : public RTA_TransactionRecordBase {
			vector<FSN_Data> Nodes;
			//for DAPI callback
			string SenderIP;
			string SenderPort;
			string PayToWallet;
		};


		// ------------ WALLET ---------------------------
		struct WALLET_PAY {
			struct request : public RTA_TransactionRecordBase {
			};
			struct response {
				string DataForClientWallet;
			};
		};

		// ---------------------------------------
		struct WALLET_GET_TRANSACTION_STATUS {
			struct request : public RTA_TransactionRecordBase {
			};
			struct response {
			};
		};

		// ---------------------------------------
		struct WALLET_PUT_TX_IN_POOL {
			struct request : public SubNetData {
				vector<string> Signs;
				vector<string> FSN_Wallets;
			};
			struct response {
			};
		};

		// ---------------------------------------
		struct WALLET_GET_POS_DATA {
			struct request : public SubNetData {
			};
			struct response {
				string DataForClientWallet;
			};
		};

		// ---------------------------------------
		struct WALLET_TR_SIGNED {
			struct request : public SubNetData {
				string Sign;
				string FSN_StakeWalletAddr;
			};
			struct response {
			};
		};



		// --------------- POS ------------------------
		struct POS_SALE {
			struct request : public RTA_TransactionRecordBase {
			};
			struct response {
			};
		};


		// ---------------------------------------
		struct POS_GET_SALE_STATUS {
			struct request : public SubNetData {
			};
			struct response {
			};
		};

		// ---------------------------------------
		struct POS_TR_SIGNED {
			struct request : public SubNetData {
				string Sign;
				string FSN_StakeWalletAddr;
			};
			struct response {
			};
		};




	};
};



#endif /* SUPERNODE_RPC_COMMAND_H_ */
