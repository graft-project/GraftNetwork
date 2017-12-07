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
		extern const string WalletTRSigned;
		extern const string WalletPutTxInPool;

		extern const string PosProxySale;
		extern const string PoSTRSigned;

		extern const string WalletGetPosData;
		extern const string WalletProxyGetPosData;

	};


	namespace rpc_command {
		extern const string DAPI_URI;//  /dapi
		extern const string DAPI_METHOD;//  GET
		extern const string DAPI_PROTOCOL;//  http for now

		// ---------------------------------------
		struct RTA_TransactionRecordRequest : public RTA_TransactionRecordBase {
			vector<string> NodesWallet;
		};



		// ------------ WALLET ---------------------------
		struct WALLET_GET_POS_DATA {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(BlockNum)
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()

				uint64_t BlockNum;

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(DataForClientWallet)
				END_KV_SERIALIZE_MAP()

				string DataForClientWallet;
			};
		};

		// ---------------------------------------
		struct WALLET_PAY {
			struct request : public RTA_TransactionRecordBase {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(POS_Wallet)
					KV_SERIALIZE(BlockNum)
					KV_SERIALIZE(Sum)
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()
			};
		};

		// ---------------------------------------
		struct WALLET_PROXY_PAY {
			struct request : public RTA_TransactionRecordRequest {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(POS_Wallet)
					KV_SERIALIZE(BlockNum)
					KV_SERIALIZE(Sum)
					KV_SERIALIZE(PaymentID)
					KV_SERIALIZE(NodesWallet)
				END_KV_SERIALIZE_MAP()
			};

			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Sign)
					KV_SERIALIZE(FSN_StakeWalletAddr)
				END_KV_SERIALIZE_MAP()

				string Sign;
				string FSN_StakeWalletAddr;
			};
		};


		// ---------------------------------------
		struct WALLET_GET_TRANSACTION_STATUS {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Status)
				END_KV_SERIALIZE_MAP()

				int Status;

			};
		};

		// ---------------------------------------
		struct WALLET_PUT_TX_IN_POOL {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Signs)
					KV_SERIALIZE(PaymentID)
					KV_SERIALIZE(FSN_Wallets)
				END_KV_SERIALIZE_MAP()

				vector<string> Signs;
				vector<string> FSN_Wallets;
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
					KV_SERIALIZE(POS_Wallet)
					KV_SERIALIZE(DataForClientWallet)
					KV_SERIALIZE(Sum)
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(BlockNum)
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()

				uint64_t BlockNum;
				string PaymentID;
			};
		};

		// ---------------------------------------
		struct POS_PROXY_SALE {
			struct request : public RTA_TransactionRecordRequest {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(SenderIP)
					KV_SERIALIZE(SenderPort)


					KV_SERIALIZE(POS_Wallet)
					KV_SERIALIZE(DataForClientWallet)
					KV_SERIALIZE(BlockNum)
					KV_SERIALIZE(Sum)
					KV_SERIALIZE(PaymentID)
					KV_SERIALIZE(NodesWallet)
				END_KV_SERIALIZE_MAP()


				//for DAPI callback
				string SenderIP;
				string SenderPort;
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
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()

			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Status)
				END_KV_SERIALIZE_MAP()

				int Status;

			};
		};

		// ---------------------------------------
		struct POS_TR_SIGNED {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Sign)
					KV_SERIALIZE(FSN_StakeWalletAddr)
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()

				string Sign;
				string FSN_StakeWalletAddr;
			};
			struct response {
				BEGIN_KV_SERIALIZE_MAP()
				END_KV_SERIALIZE_MAP()

			};
		};


		void ConvertFromTR(RTA_TransactionRecordRequest& dst, const RTA_TransactionRecord& src);

		void ConvertToTR(RTA_TransactionRecord& dst, const RTA_TransactionRecordRequest& src, const FSN_ServantBase* servant);


	};
};

//namespace tools {
//namespace supernode_rpc {

//	struct COMMAND_RPC_EMPTY_TEST {
//		struct request {
//			BEGIN_KV_SERIALIZE_MAP()
//			END_KV_SERIALIZE_MAP()
//		};

//		struct response {
//			BEGIN_KV_SERIALIZE_MAP()
//			END_KV_SERIALIZE_MAP()
//		};
//	};

//    //Wallet DAPI
//    struct COMMAND_RPC_READY_TO_PAY
//    {
//      struct request
//      {
//        std::string pid;
//        std::string key_image;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(pid)
//          KV_SERIALIZE(key_image)
//        END_KV_SERIALIZE_MAP()
//      };

//      struct response
//      {
//        int64_t result;
//        std::string transaction;
//        std::string data;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(result)
//          KV_SERIALIZE(transaction)
//          KV_SERIALIZE(data)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    struct COMMAND_RPC_REJECT_PAY
//    {
//      struct request
//      {
//        std::string pid;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(pid)
//        END_KV_SERIALIZE_MAP()
//      };

//      struct response
//      {
//        int64_t result;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(result)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    struct transaction_record
//    {
//      uint64_t amount;
//      std::string address;

//      BEGIN_KV_SERIALIZE_MAP()
//        KV_SERIALIZE(amount)
//        KV_SERIALIZE(address)
//      END_KV_SERIALIZE_MAP()
//    };

//    struct COMMAND_RPC_PAY
//    {
//      struct request
//      {
//        std::string pid;
//        transaction_record transaction;
//        std::string account;
//        std::string password;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(pid)
//          KV_SERIALIZE(transaction)
//          KV_SERIALIZE(account)
//          KV_SERIALIZE(password)
//        END_KV_SERIALIZE_MAP()
//      };

//      struct response
//      {
//        int64_t result;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(result)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    struct COMMAND_RPC_GET_PAY_STATUS
//    {
//      struct request
//      {
//        std::string pid;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(pid)
//        END_KV_SERIALIZE_MAP()
//      };

//      struct response
//      {
//        int64_t result;
//        int64_t pay_status;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(result)
//          KV_SERIALIZE(pay_status)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    //Point of Sale DAPI
//    struct COMMAND_RPC_SALE
//    {
//      struct request
//      {
//        std::string pid;
//        std::string data;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(pid)
//          KV_SERIALIZE(data)
//        END_KV_SERIALIZE_MAP()
//      };

//      struct response
//      {
//        int64_t result;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(result)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    struct COMMAND_RPC_GET_SALE_STATUS
//    {
//      struct request
//      {
//        std::string pid;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(pid)
//        END_KV_SERIALIZE_MAP()
//      };

//      struct response
//      {
//        int64_t result;
//        int64_t sale_status;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(result)
//          KV_SERIALIZE(sale_status)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    //Generic DAPI
//    struct COMMAND_RPC_GET_WALLET_BALANCE
//    {
//      struct request
//      {
//        std::string account;
//        std::string password;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(account)
//          KV_SERIALIZE(password)
//        END_KV_SERIALIZE_MAP()
//      };

//      struct response
//      {
//        uint64_t balance;
//        uint64_t unlocked_balance;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(balance)
//          KV_SERIALIZE(unlocked_balance)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    struct COMMAND_RPC_GET_SUPERNODE_LIST
//    {
//      struct request
//      {
//        BEGIN_KV_SERIALIZE_MAP()
//        END_KV_SERIALIZE_MAP()
//      };

//      struct response
//      {
//        int64_t result;
//        std::list<std::string> supernode_list;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(result)
//          KV_SERIALIZE(supernode_list)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    //Temporal DAPI
//    struct COMMAND_RPC_CREATE_ACCOUNT
//    {
//      struct request
//      {
//        std::string password;
//        std::string language;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(password)
//          KV_SERIALIZE(language)
//        END_KV_SERIALIZE_MAP()
//      };
//      struct response
//      {
//        std::string address;
//        std::string account;

//        BEGIN_KV_SERIALIZE_MAP()
//          KV_SERIALIZE(address)
//          KV_SERIALIZE(account)
//        END_KV_SERIALIZE_MAP()
//      };
//    };

//    struct COMMAND_RPC_GET_PAYMENT_ADDRESS
//    {
//      struct request
//      {
//          std::string account;
//          std::string password;
//          std::string payment_id;

//          BEGIN_KV_SERIALIZE_MAP()
//            KV_SERIALIZE(account)
//            KV_SERIALIZE(password)
//            KV_SERIALIZE(payment_id)
//          END_KV_SERIALIZE_MAP()
//      };
//      struct response
//      {
//          std::string payment_address;
//          std::string payment_id;

//          BEGIN_KV_SERIALIZE_MAP()
//            KV_SERIALIZE(payment_address)
//            KV_SERIALIZE(payment_id)
//          END_KV_SERIALIZE_MAP()
//      };
//    };

//    struct COMMAND_RPC_GET_SEED
//    {
//      struct request
//      {
//          std::string account;
//          std::string password;
//          std::string language;

//          BEGIN_KV_SERIALIZE_MAP()
//            KV_SERIALIZE(account)
//            KV_SERIALIZE(password)
//            KV_SERIALIZE(language)
//          END_KV_SERIALIZE_MAP()
//      };
//      struct response
//      {
//          std::string seed;

//          BEGIN_KV_SERIALIZE_MAP()
//            KV_SERIALIZE(seed)
//          END_KV_SERIALIZE_MAP()
//      };
//    };

//    struct COMMAND_RPC_RESTORE_ACCOUNT
//    {
//      struct request
//      {
//          std::string seed;
//          std::string password;

//          BEGIN_KV_SERIALIZE_MAP()
//            KV_SERIALIZE(seed)
//            KV_SERIALIZE(password)
//          END_KV_SERIALIZE_MAP()
//      };
//      struct response
//      {
//          std::string address;
//          std::string account;

//          BEGIN_KV_SERIALIZE_MAP()
//            KV_SERIALIZE(address)
//            KV_SERIALIZE(account)
//          END_KV_SERIALIZE_MAP()
//      };
//    };
//}
//}

#endif /* SUPERNODE_RPC_COMMAND_H_ */
