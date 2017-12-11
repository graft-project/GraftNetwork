#ifndef SUPERNODE_RPC_COMMAND_H_
#define SUPERNODE_RPC_COMMAND_H_

#include "supernode_common_struct.h"


namespace supernode {
	class FSN_ServantBase;

	namespace dapi_call {
		extern const string Pay;

		extern const string GetPayStatus;
		extern const string Sale;
        extern const string PosRejectSale;
		extern const string GetSaleStatus;

		extern const string WalletProxyPay;
		extern const string WalletTRSigned;
		extern const string WalletPutTxInPool;

		extern const string PosProxySale;
		extern const string PoSTRSigned;

		extern const string WalletGetPosData;
		extern const string WalletProxyGetPosData;

        extern const string GetWalletBalance;

        extern const string CreateAccount;
        extern const string GetSeed;
        extern const string RestoreAccount;

        extern const string WalletRejectPay;
        extern const string WalletProxyRejectPay;
        extern const string AuthWalletRejectPay;
	};


	namespace rpc_command {
		extern const string DAPI_URI;//  /dapi
        extern const string DAPI_METHOD;//  POST
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
                int64_t Result;
                string DataForClientWallet;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
					KV_SERIALIZE(DataForClientWallet)
				END_KV_SERIALIZE_MAP()

			};
		};

		// ---------------------------------------
		struct WALLET_PAY {
			struct request : public RTA_TransactionRecordBase {
                std::string Account;
                std::string Password;

				BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Account)
                    KV_SERIALIZE(Password)
					KV_SERIALIZE(POS_Wallet)
					KV_SERIALIZE(BlockNum)
					KV_SERIALIZE(Sum)
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()
			};
			struct response {
                int64_t Result;

				BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
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
        struct WALLET_REJECT_PAY {
            struct request : public SubNetData {
                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(PaymentID)
                    KV_SERIALIZE(BlockNum)
                END_KV_SERIALIZE_MAP()

                uint64_t BlockNum;
            };
            struct response {
                int64_t Result;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                END_KV_SERIALIZE_MAP()
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
                int64_t Result;
                int Status;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
					KV_SERIALIZE(Status)
				END_KV_SERIALIZE_MAP()
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
                int64_t Result;
                uint64_t BlockNum;
                string PaymentID;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
					KV_SERIALIZE(BlockNum)
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()
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
        struct POS_REJECT_SALE {
            struct request : public SubNetData {
                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(PaymentID)
                END_KV_SERIALIZE_MAP()

            };
            struct response {
                int64_t Result;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
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
                int64_t Result;
                int Status;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
					KV_SERIALIZE(Status)
				END_KV_SERIALIZE_MAP()
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


        // ------------ Generic -------------------
        struct GET_WALLET_BALANCE {
            struct request {
                std::string Account;
                std::string Password;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Account)
                    KV_SERIALIZE(Password)
                END_KV_SERIALIZE_MAP()
            };
            struct response {
                int64_t Result;
                uint64_t Balance;
                uint64_t UnlockedBalance;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                    KV_SERIALIZE(Balance)
                    KV_SERIALIZE(UnlockedBalance)
                END_KV_SERIALIZE_MAP()
            };
        };

        // ----------- Temporal ------------------
        struct CREATE_ACCOUNT {
            struct request {
                std::string Password;
                std::string Language;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Password)
                    KV_SERIALIZE(Language)
                END_KV_SERIALIZE_MAP()
            };
            struct response {
                int64_t Result;
                std::string Address;
                std::string Account;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                    KV_SERIALIZE(Address)
                    KV_SERIALIZE(Account)
                END_KV_SERIALIZE_MAP()
            };
        };

        struct GET_SEED {
            struct request {
                std::string Account;
                std::string Password;
                std::string Language;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Account)
                    KV_SERIALIZE(Password)
                    KV_SERIALIZE(Language)
                END_KV_SERIALIZE_MAP()
            };
            struct response {
                int64_t Result;
                std::string Seed;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                    KV_SERIALIZE(Seed)
                END_KV_SERIALIZE_MAP()
            };
        };

        struct RESTORE_ACCOUNT {
            struct request {
                std::string Seed;
                std::string Password;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Seed)
                    KV_SERIALIZE(Password)
                END_KV_SERIALIZE_MAP()
            };
            struct response {
                int64_t Result;
                std::string Address;
                std::string Account;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                    KV_SERIALIZE(Address)
                    KV_SERIALIZE(Account)
                END_KV_SERIALIZE_MAP()
            };
        };

		void ConvertFromTR(RTA_TransactionRecordRequest& dst, const RTA_TransactionRecord& src);

		void ConvertToTR(RTA_TransactionRecord& dst, const RTA_TransactionRecordRequest& src, const FSN_ServantBase* servant);
    }
}

#endif /* SUPERNODE_RPC_COMMAND_H_ */
