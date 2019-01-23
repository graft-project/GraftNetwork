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

#ifndef SUPERNODE_RPC_COMMAND_H_
#define SUPERNODE_RPC_COMMAND_H_

#include "supernode_common_struct.h"
#include "serialization/keyvalue_serialization.h"
#include "storages/portable_storage_base.h"


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
        extern const string Transfer;
        extern const string GetTransferFee;

        extern const string WalletRejectPay;
        extern const string WalletProxyRejectPay;
        extern const string AuthWalletRejectPay;

        extern const string FSN_CheckWalletOwnership;
	};

	namespace p2p_call {
		extern const string AddFSN;
		extern const string LostFSNStatus;
		extern const string GetFSNList;
		extern const string AddSeed;
		extern const string GetSeedsList;
	};

	namespace rpc_command {
		extern const string DAPI_URI;//  /dapi
        extern const string DAPI_METHOD;//  POST
		extern const string DAPI_PROTOCOL;//  http for now
		extern const string DAPI_VERSION;

		bool IsWalletProxyOnly();
		void SetWalletProxyOnly(bool b);

		void SetDAPIVersion(const string& v);

	    template<typename t_param>
	    struct RequestContainer {
	      std::string jsonrpc;
	      std::string method;
	      std::string dapi_version;
	      epee::serialization::storage_entry id;
	      t_param     params;

	      RequestContainer() {
	    	  jsonrpc = "2.0";
	    	  id = epee::serialization::storage_entry(0);
	    	  dapi_version = DAPI_VERSION;

	      }

	      BEGIN_KV_SERIALIZE_MAP()
	        KV_SERIALIZE(jsonrpc)
	        KV_SERIALIZE(id)
			KV_SERIALIZE(dapi_version)
	        KV_SERIALIZE(method)
	        KV_SERIALIZE(params)
	      END_KV_SERIALIZE_MAP()
	    };



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
                string POSSaleDetails;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                    KV_SERIALIZE(POSSaleDetails)
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
                    KV_SERIALIZE(POSAddress)
					KV_SERIALIZE(BlockNum)
                    KV_SERIALIZE(Amount)
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
                    KV_SERIALIZE(POSAddress)
					KV_SERIALIZE(BlockNum)
                    KV_SERIALIZE(Amount)
					KV_SERIALIZE(PaymentID)
					KV_SERIALIZE(NodesWallet)
					KV_SERIALIZE(CustomerWalletAddr)
					KV_SERIALIZE(CustomerWalletSign)
				END_KV_SERIALIZE_MAP()

				string CustomerWalletAddr;
				string CustomerWalletSign;
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
					KV_SERIALIZE(PaymentID)
					KV_SERIALIZE(TransactionPoolID)
				END_KV_SERIALIZE_MAP()

				string TransactionPoolID;
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
                    KV_SERIALIZE(POSAddress)
                    KV_SERIALIZE(POSViewKey)
                    KV_SERIALIZE(POSSaleDetails)
                    KV_SERIALIZE(Amount)
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


                    KV_SERIALIZE(POSAddress)
                    KV_SERIALIZE(POSSaleDetails)
					KV_SERIALIZE(BlockNum)
                    KV_SERIALIZE(Amount)
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
					KV_SERIALIZE(PaymentID)
					KV_SERIALIZE(TransactionPoolID);
				END_KV_SERIALIZE_MAP()
				string TransactionPoolID;
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
                std::string ViewKey;
                std::string Account;
                std::string Seed;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                    KV_SERIALIZE(Address)
                    KV_SERIALIZE(ViewKey)
                    KV_SERIALIZE(Account)
                    KV_SERIALIZE(Seed)
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
                std::string ViewKey;
                std::string Account;
                std::string Seed;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                    KV_SERIALIZE(Address)
                    KV_SERIALIZE(ViewKey)
                    KV_SERIALIZE(Account)
                    KV_SERIALIZE(Seed)
                END_KV_SERIALIZE_MAP()
            };
        };

        struct TRANSFER {
            struct request {
                std::string Account;
                std::string Password;
                std::string Address;
                std::string PaymentID;
                std::string Amount;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Account)
                    KV_SERIALIZE(Password)
                    KV_SERIALIZE(Address)
                    KV_SERIALIZE(PaymentID)
                    KV_SERIALIZE(Amount)
                END_KV_SERIALIZE_MAP()
            };
            struct response {
                int64_t Result;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                END_KV_SERIALIZE_MAP()
            };
        };


        struct GET_TRANSFER_FEE {
            struct request {
                std::string Account;
                std::string Password;
                std::string Address;
                std::string PaymentID;
                std::string Amount;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Account)
                    KV_SERIALIZE(Password)
                    KV_SERIALIZE(Address)
                    KV_SERIALIZE(PaymentID)
                    KV_SERIALIZE(Amount)
                END_KV_SERIALIZE_MAP()
            };
            struct response {
                int64_t Result;
                uint64_t Fee;

                BEGIN_KV_SERIALIZE_MAP()
                    KV_SERIALIZE(Result)
                    KV_SERIALIZE(Fee)
                END_KV_SERIALIZE_MAP()
            };
        };

		void ConvertFromTR(RTA_TransactionRecordRequest& dst, const RTA_TransactionRecord& src);

		void ConvertToTR(RTA_TransactionRecord& dst, const RTA_TransactionRecordRequest& src, const FSN_ServantBase* servant);

		// -----------------------------------------------
		struct P2P_DUMMY_RESP {
			BEGIN_KV_SERIALIZE_MAP()
			END_KV_SERIALIZE_MAP()
		};

		struct BROADCACT_ADD_FULL_SUPER_NODE : public SubNetData {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(IP)
				KV_SERIALIZE(Port)
				KV_SERIALIZE(PaymentID)

				KV_SERIALIZE(StakeAddr)
				KV_SERIALIZE(StakeViewKey)
				KV_SERIALIZE(MinerAddr)
				KV_SERIALIZE(MinerViewKey)
			END_KV_SERIALIZE_MAP()

			string IP;
			string Port;

			string StakeAddr;
			string StakeViewKey;
			string MinerAddr;
			string MinerViewKey;
		};

		struct BROADCACT_LOST_STATUS_FULL_SUPER_NODE : public SubNetData {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(StakeAddr)
				KV_SERIALIZE(PaymentID)
			END_KV_SERIALIZE_MAP()


			string StakeAddr;
		};


		struct FSN_CHECK_WALLET_OWNERSHIP {
			struct request {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(WalletAddr)
					KV_SERIALIZE(Str)
				END_KV_SERIALIZE_MAP()

				string WalletAddr;
				string Str;
			};

			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(Sign)
				END_KV_SERIALIZE_MAP()

				string Sign;// sign ip:port:wallet addr by wallet private key
			};
		};

		struct BROADCAST_NEAR_GET_ACTUAL_FSN_LIST {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()
			};

			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(List)
				END_KV_SERIALIZE_MAP()

				vector<BROADCACT_ADD_FULL_SUPER_NODE> List;
			};
		};

		struct P2P_ADD_NODE_TO_LIST : public SubNetData {
			BEGIN_KV_SERIALIZE_MAP()
				KV_SERIALIZE(PaymentID)
				KV_SERIALIZE(IP)
				KV_SERIALIZE(Port)
			END_KV_SERIALIZE_MAP()

			string IP;
			string Port;
		};


		struct P2P_GET_ALL_NODES_LIST {
			struct request : public SubNetData {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(PaymentID)
				END_KV_SERIALIZE_MAP()
			};

			struct response {
				BEGIN_KV_SERIALIZE_MAP()
					KV_SERIALIZE(List)
				END_KV_SERIALIZE_MAP()

				vector<P2P_ADD_NODE_TO_LIST> List;
			};
		};
    }
}

#endif /* SUPERNODE_RPC_COMMAND_H_ */
