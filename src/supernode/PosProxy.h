#ifndef POS_PROXY_H_
#define POS_PROXY_H_

#include "PosSaleObject.h"
#include "BaseRTAProcessor.h"

namespace supernode {
	class PosProxy : public BaseRTAProcessor {
		public:

		protected:
		void Init() override;

		bool Sale(const rpc_command::POS_SALE::request& in, rpc_command::POS_SALE::response& out);


	};

}

//#pragma	once

//#include <boost/program_options/options_description.hpp>
//#include <boost/program_options/variables_map.hpp>
//#include <string>
//#include "supernode_rpc_server_commands_defs.h"
//#include "net/http_server_impl_base.h"
//#include "common/command_line.h"
//#include "net/http_client.h"
//#include "graft_wallet.h"

//class DataStorage;

//namespace
//{
//const command_line::arg_descriptor<std::string, true> arg_rpc_bind_port = {"rpc-bind-port", "Sets bind port for server"};
//const command_line::arg_descriptor<bool> arg_disable_rpc_login = {"disable-rpc-login", "Disable HTTP authentication for RPC connections served by this process"};
//const command_line::arg_descriptor<bool> arg_trusted_daemon = {"trusted-daemon", "Enable commands which rely on a trusted daemon", false};

//constexpr const char default_rpc_username[] = "graft";
//}

//namespace tools {
//class supernode_rpc_server: public epee::http_server_impl_base<supernode_rpc_server> {
//public:
//    typedef epee::net_utils::connection_context_base connection_context;

//public:
//    supernode_rpc_server();
//    ~supernode_rpc_server();

//    bool init(const boost::program_options::variables_map *vm);

//    static const char* tr(const char* str);

//private:
//    CHAIN_HTTP_TO_MAP2(connection_context) //forward http requests to uri map

//    BEGIN_URI_MAP2()
//    BEGIN_JSON_RPC_MAP("/json_rpc")
//        //Test DAPI
//        MAP_JON_RPC_WE("test_call",         on_test_call,       supernode_rpc::COMMAND_RPC_EMPTY_TEST)
//        //Wallet DAPI
//        MAP_JON_RPC_WE("ReadyToPay",        onReadyToPay,       supernode_rpc::COMMAND_RPC_READY_TO_PAY)
//        MAP_JON_RPC_WE("RejectPay",         onRejectPay,        supernode_rpc::COMMAND_RPC_REJECT_PAY)
//        MAP_JON_RPC_WE("Pay",               onPay,              supernode_rpc::COMMAND_RPC_PAY)
//        MAP_JON_RPC_WE("GetPayStatus",      onGetPayStatus,     supernode_rpc::COMMAND_RPC_GET_PAY_STATUS)
//        //Point of Sale DAPI
//        MAP_JON_RPC_WE("Sale",              onSale,             supernode_rpc::COMMAND_RPC_SALE)
//        MAP_JON_RPC_WE("GetSaleStatus",     onGetSaleStatus,    supernode_rpc::COMMAND_RPC_GET_SALE_STATUS)
//        //Generic DAPI
//        MAP_JON_RPC_WE("GetWalletBalance",  onGetWalletBalance, supernode_rpc::COMMAND_RPC_GET_WALLET_BALANCE)
//        MAP_JON_RPC_WE("GetSupernodeList",  onGetSupernodeList, supernode_rpc::COMMAND_RPC_GET_SUPERNODE_LIST)
//        //Temporal DAPI
//        MAP_JON_RPC_WE("CreateAccount",     onCreateAccount,    supernode_rpc::COMMAND_RPC_CREATE_ACCOUNT)
//        MAP_JON_RPC_WE("GetPaymentAddress", onGetPaymentAddress,supernode_rpc::COMMAND_RPC_GET_PAYMENT_ADDRESS)
//        MAP_JON_RPC_WE("GetSeed",           onGetSeed,          supernode_rpc::COMMAND_RPC_GET_SEED)
//        MAP_JON_RPC_WE("RestoreAccount",    onRestoreAccount,   supernode_rpc::COMMAND_RPC_RESTORE_ACCOUNT)
//    END_JSON_RPC_MAP()
//    END_URI_MAP2()

//    //JSON-RPC: Wallet DAPI
//    bool onReadyToPay(const supernode_rpc::COMMAND_RPC_READY_TO_PAY::request &req, supernode_rpc::COMMAND_RPC_READY_TO_PAY::response &res, epee::json_rpc::error &er);
//    bool onRejectPay(const supernode_rpc::COMMAND_RPC_REJECT_PAY::request &req, supernode_rpc::COMMAND_RPC_REJECT_PAY::response &res, epee::json_rpc::error &er);
//    bool onPay(const supernode_rpc::COMMAND_RPC_PAY::request &req, supernode_rpc::COMMAND_RPC_PAY::response &res, epee::json_rpc::error &er);
//    bool onGetPayStatus(const supernode_rpc::COMMAND_RPC_GET_PAY_STATUS::request &req, supernode_rpc::COMMAND_RPC_GET_PAY_STATUS::response &res, epee::json_rpc::error &er);
//    //JSON-RPC: Point of Sale DAPI
//    bool onSale(const supernode_rpc::COMMAND_RPC_SALE::request &req, supernode_rpc::COMMAND_RPC_SALE::response &res, epee::json_rpc::error &er);
//    bool onGetSaleStatus(const supernode_rpc::COMMAND_RPC_GET_SALE_STATUS::request &req, supernode_rpc::COMMAND_RPC_GET_SALE_STATUS::response &res, epee::json_rpc::error &er);
//    //JSON-RPC: Generic DAPI
//    bool onGetWalletBalance(const supernode_rpc::COMMAND_RPC_GET_WALLET_BALANCE::request &req, supernode_rpc::COMMAND_RPC_GET_WALLET_BALANCE::response &res, epee::json_rpc::error &er);
//    bool onGetSupernodeList(const supernode_rpc::COMMAND_RPC_GET_SUPERNODE_LIST::request &req, supernode_rpc::COMMAND_RPC_GET_SUPERNODE_LIST::response &res, epee::json_rpc::error &er);
//    //JSON-RPC: Temporal DAPI
//    bool onCreateAccount(const supernode_rpc::COMMAND_RPC_CREATE_ACCOUNT::request &req, supernode_rpc::COMMAND_RPC_CREATE_ACCOUNT::response &res, epee::json_rpc::error &er);
//    bool onGetPaymentAddress(const supernode_rpc::COMMAND_RPC_GET_PAYMENT_ADDRESS::request &req, supernode_rpc::COMMAND_RPC_GET_PAYMENT_ADDRESS::response &res, epee::json_rpc::error &er);
//    bool onGetSeed(const supernode_rpc::COMMAND_RPC_GET_SEED::request &req, supernode_rpc::COMMAND_RPC_GET_SEED::response &res, epee::json_rpc::error &er);
//    bool onRestoreAccount(const supernode_rpc::COMMAND_RPC_RESTORE_ACCOUNT::request &req, supernode_rpc::COMMAND_RPC_RESTORE_ACCOUNT::response &res, epee::json_rpc::error &er);

//    std::unique_ptr<tools::GraftWallet> initWallet(const std::string &account, const std::string &password, epee::json_rpc::error &er) const;
//    std::unique_ptr<tools::GraftWallet> createEmptyWallet(const std::string &password, epee::json_rpc::error &er) const;

//private:
//    bool m_trusted_daemon;
//    std::string rpc_login_filename;
//    epee::net_utils::http::http_simple_client m_http_client;
//    const boost::program_options::variables_map *m_vm;
//    DataStorage *m_trans_status_storage;
//    DataStorage *m_trans_cache_storage;
//    DataStorage *m_auth_cache_storage;
//};
//}

#endif /* POS_PROXY_H_ */
