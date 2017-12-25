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

#ifndef BASE_RTA_PROCESSOR_H_
#define BASE_RTA_PROCESSOR_H_

#include "BaseRTAObject.h"

namespace supernode {

	class BaseRTAProcessor {
		public:
		virtual ~BaseRTAProcessor();

		virtual void Start();
		virtual void Stop();

		void Set(const FSN_ServantBase* ser, DAPI_RPC_Server* dapi);
		virtual void Tick();

		protected:
		void Add(boost::shared_ptr<BaseRTAObject> obj);
		void Remove(boost::shared_ptr<BaseRTAObject> obj);
		void Setup(boost::shared_ptr<BaseRTAObject> obj);
		boost::shared_ptr<BaseRTAObject> ObjectByPayment(const string& payment_id);

        virtual void Init() = 0;

		protected:
		const FSN_ServantBase* m_Servant = nullptr;
		DAPI_RPC_Server* m_DAPIServer = nullptr;
		mutable boost::recursive_mutex m_ObjectsGuard;
		vector< boost::shared_ptr<BaseRTAObject> > m_Objects;

		mutable boost::recursive_mutex m_RemoveObjectsGuard;
		vector< boost::shared_ptr<BaseRTAObject> > m_RemoveObjects;

//		mutable boost::mutex m_DeleteGuard;
//		map< boost::posix_time::ptime, boost::shared_ptr<BaseRTAObject> > m_ForDelete;
		// boost::posix_time::second_clock::local_time()

	};

}

#endif /* BASE_RTA_PROCESSOR_H_ */
