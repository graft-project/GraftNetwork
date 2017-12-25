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

#ifndef FSN_SERVANTBASE_H_H_H_
#define FSN_SERVANTBASE_H_H_H_

#include "supernode_common_struct.h"

namespace supernode {
	class FSN_ServantBase {
	public:
		virtual ~FSN_ServantBase();

	    virtual vector<pair<uint64_t, boost::shared_ptr<FSN_Data>>>
	    LastBlocksResolvedByFSN(uint64_t startFromBlock, uint64_t blockNums) const=0;

	    virtual vector<boost::shared_ptr<FSN_Data>> GetAuthSample(uint64_t forBlockNum) const=0;

	    virtual uint64_t GetCurrentBlockHeight() const=0;

	    virtual string SignByWalletPrivateKey(const string& str, const string& wallet_addr) const=0;

	    virtual bool IsSignValid(const string& message, const string &address, const string &signature) const=0;


	    virtual uint64_t GetWalletBalance(uint64_t block_num, const FSN_WalletData& wallet) const=0;

	public:
	    // Add WITHOUT any checks. And child add WITHOUT any checks for stake, ping or any other req FSN attrs
	    virtual void AddFsnAccount(boost::shared_ptr<FSN_Data> fsn);
	    virtual bool RemoveFsnAccount(boost::shared_ptr<FSN_Data> fsn);
	    virtual boost::shared_ptr<FSN_Data> FSN_DataByStakeAddr(const string& addr) const;
        /*!
         * \brief GetNodeIp - returns IP addess of node
         * \return          string
         */
        std::string GetNodeIp() const;
        /*!
         * \brief GetNodePort - returns TCP port number of node
         * \return
         */
        int GetNodePort() const;

        /*!
         * \brief GetNodeAddress - returns
         * \return
         */
        std::string GetNodeAddress() const;

        /*!
         * \brief GetNodeLogin - login for remote node
         * \return
         */
        std::string GetNodeLogin() const;

        /*!
         * \brief GetNodePassword -  password for remote node
         * \return
         */
        std::string GetNodePassword() const;

        /*!
         * \brief IsTestnet - indicates if
         * \return
         */
        bool IsTestnet() const;

    protected:
        /*!
         * \brief SetNodeAddress - accepts address string in form "hostname:port"
         * \param address - node address
         */
        void SetNodeAddress(const std::string &address);



	public:
	    virtual FSN_WalletData GetMyStakeWallet() const=0;
	    virtual FSN_WalletData GetMyMinerWallet() const=0;
	    virtual unsigned AuthSampleSize() const=0;


	public:
	    mutable boost::recursive_mutex All_FSN_Guard;// DO NOT block for long time. if need - use copy
	    vector< boost::shared_ptr<FSN_Data> > All_FSN;// access to this data may be done from different threads

    protected:
        bool  m_testnet = false;
        // IP address for access to graft node
        std::string m_nodeIp;
        // TCP port  for access to graft node
        int m_nodePort;
        std::string m_nodelogin;
        std::string m_nodePassword;
	};


}



#endif
