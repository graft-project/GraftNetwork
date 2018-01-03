/*
 * WorkerPool.cpp
 *
 *  Created on: Jan 2, 2018
 *      Author: laid
 */

#include <supernode/WorkerPool.h>

namespace supernode {

WorkerPool::WorkerPool() : Work(Service) {
}


void WorkerPool::Workers(int cnt) {
	for(int i=0;i<cnt;i++) {
		Threadpool.create_thread( boost::bind(&boost::asio::io_service::run, &Service) );
	}
}

void WorkerPool::Stop() {
	Service.stop();
	Threadpool.join_all();
}

} /* namespace supernode */
