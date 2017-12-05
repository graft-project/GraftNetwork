#ifndef SUPERNODE_RPC_COMMAND_H_
#define SUPERNODE_RPC_COMMAND_H_

#include "supernode_common_struct.h"


namespace supernode {
	class FSN_ServantBase;

	namespace dapi_call {
		extern const string Pay;
		extern const string GetPayStatus;
		extern const string Sale;
		extern const string GetSaleStatus;

		extern const string WalletProxyPay;
		extern const string WalletGetPosData;
		extern const string WalletTRSigned;
		extern const string WalletPutTxInPool;

		extern const string PosProxySale;
		extern const string PoSTRSigned;
	};

	namespace rpc_command {
		extern const string DAPI_URI;//  /dapi
		extern const string DAPI_METHOD;//  GET
		extern const string DAPI_PROTOCOL;//  http for now

		// ---------------------------------------
		struct SUB_NET_BROADCAST_RESPONCE {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(Fail)
			END_KV_SERIALIZE_MAP()

			bool Fail;
		};
		struct RTA_TRANSACTION_OBJECT {
			struct request : public RTA_TransactionRecordBase {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(SenderIP)
					KV_SERIALIZE(SenderPort)
					KV_SERIALIZE(PayToWallet)
					END_KV_SERIALIZE_MAP()

					vector<FSN_Data> Nodes;
				//for DAPI callback
				string SenderIP;
				string SenderPort;
				string PayToWallet;
			};

			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()
			};

		};


		// ------------ WALLET ---------------------------
		struct WALLET_PAY {
			struct request : public RTA_TransactionRecordBase {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(DataForClientWallet)
				END_KV_SERIALIZE_MAP()

				string DataForClientWallet;
			};
		};

		// ---------------------------------------
		struct WALLET_GET_TRANSACTION_STATUS {
			struct request : public RTA_TransactionRecordBase {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
		};

		// ---------------------------------------
		struct WALLET_PUT_TX_IN_POOL {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Signs)
				END_KV_SERIALIZE_MAP()

				vector<string> Signs;
				vector<string> FSN_Wallets;
			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
		};

		// ---------------------------------------
		struct WALLET_GET_POS_DATA {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(DataForClientWallet)
				END_KV_SERIALIZE_MAP()

				string DataForClientWallet;
			};
		};

		// ---------------------------------------
		struct WALLET_TR_SIGNED {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Sign)
					KV_SERIALIZE(FSN_StakeWalletAddr)
				END_KV_SERIALIZE_MAP()

				string Sign;
				string FSN_StakeWalletAddr;
			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
		};



		// --------------- POS ------------------------
		struct POS_SALE {
			struct request : public RTA_TransactionRecordBase {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
		};


		// ---------------------------------------
		struct POS_GET_SALE_STATUS {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
		};

		// ---------------------------------------
		struct POS_TR_SIGNED {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Sign)
					KV_SERIALIZE(FSN_StakeWalletAddr)
				END_KV_SERIALIZE_MAP()

				string Sign;
				string FSN_StakeWalletAddr;
			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
		};


		void ConvertFromTR(RTA_TRANSACTION_OBJECT::request& dst, const RTA_TransactionRecord& src);

		bool ConvertToTR(RTA_TransactionRecord& dst, const RTA_TRANSACTION_OBJECT::request& src, const FSN_ServantBase* servant);


	};
};



#endif /* SUPERNODE_RPC_COMMAND_H_ */
