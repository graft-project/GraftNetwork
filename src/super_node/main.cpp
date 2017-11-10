
#include "super_node_rpc_server.h"


int main(int argc, char** argv) {
	LOG_PRINT_L0("=1");
	tools::super_node_rpc_server rpc;
	rpc.init( "7655", "127.0.0.1");
	LOG_PRINT_L0("=2");
	rpc.run(100);
	LOG_PRINT_L0("=3");

	return 0;
}



