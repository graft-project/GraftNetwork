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

#ifndef FSN_SERVANT_H_
#define FSN_SERVANT_H_

#include "FSN_ServantBase.h"
#include <cryptonote_core/cryptonote_core.h>
#include <wallet/api/wallet2_api.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <string>
using namespace std;

namespace supernode {

class FSN_Servant : public FSN_ServantBase
{
public:

    // TODO: remove it;
    FSN_Servant();
    FSN_Servant(const FSN_Servant &other);
    /*!
     * \brief FSN_Servant - ctor
     * \param bdb_path    - path to blockchain db
     * \param node_addr   - node address in form "hostname:port"
     * \param fsn_wallets_dir - directory where to store FSN wallets pair (miner/stake)
     * \param testnet    -  testnet flag
     */
    // TODO: add credentials for the node
    FSN_Servant(const string &bdb_path, const string &node_addr, const string &node_login, const string &node_password,
                const string &fsn_wallets_dir, bool testnet = false);
    // data for my wallet access
    void Set(const string& stakeFileName, const string& stakePasswd, const string& minerFileName, const string& minerPasswd);
    // start from blockchain top and check, if block solved by one from  full_super_node_servant::all_fsn
    // push block number and full_super_node_data to output vector. stop, when output_vector.size==blockNums OR blockchain ends
    // output_vector.begin - newest block, solved by FSN
    // may be called from different threads.
    /*!
     * \brief LastBlocksResolvedByFSN - returns vector of supernodes who solved blocks
     *
     * start from blockchain top and check, if block solved by one from  full_super_node_servant::all_fsn
     * push block number and full_super_node_data to output vector. stop, when output_vector.size==blockNums OR blockchain ends
     * output_vector.begin - newest block, solved by FSN
     * may be called from different threads.
     *
     * \param startFromBlock
     * \param blockNums
     * \return
     */
    vector<pair<uint64_t, boost::shared_ptr<FSN_Data>>>
    LastBlocksResolvedByFSN(uint64_t startFromBlock, uint64_t blockNums) const override;

    // start scan blockchain from forBlockNum, scan from top to bottom
    vector<boost::shared_ptr<FSN_Data>> GetAuthSample(uint64_t forBlockNum) const  override;
    /*!
     * \brief GetCurrentBlockNum - returns current blockchain height
     * \return
     */
    uint64_t GetCurrentBlockHeight() const  override;

    /*!
     * \brief SignByWalletPrivateKey - signs the message with the given wallet address.
     *                                 is given address is not miner nor stake wallet address,
     *                                 throws std::runtime error
     * \param str                    - message
     * \param wallet_addr            - miner or stake wallet address
     * \return                       - signature
     */
    string SignByWalletPrivateKey(const string& str, const string& wallet_addr) const  override;

    /*!
     * \brief IsSignValid - checks signature for given message and wallet address
     * \param message     - message
     * \param address     - wallet address
     * \param signature   - signature
     * \return            - true if signature is valid
     */
    bool IsSignValid(const string& message, const string &address, const string &signature) const  override;

    // calc balance from chain begin to block_num
    uint64_t GetWalletBalance(uint64_t block_num, const FSN_WalletData& wallet) const  override;

    virtual void AddFsnAccount(boost::shared_ptr<FSN_Data> fsn) override;
    virtual bool RemoveFsnAccount(boost::shared_ptr<FSN_Data> fsn) override;


public:
    FSN_WalletData GetMyStakeWallet() const  override;
    FSN_WalletData GetMyMinerWallet() const  override;
    unsigned AuthSampleSize() const override;

private:
    static bool proofCoinbaseTx(const cryptonote::account_public_address &address, const cryptonote::block &block,
                         const crypto::secret_key &viewkey);
    bool initBlockchain(const std::string &dbpath, bool testnet);

    Monero::Wallet * initWallet(Monero::Wallet *existingWallet, const string &path, const string &password, bool testnet);
    /*!
     * \brief initViewOnlyWallet - creates new or opens existing view only wallet
     * \brief walletData         - address and viewkey
     * \param testnet            - testnet flag
     * \return                   - pointer to Monero::Wallet obj
     */
    Monero::Wallet * initViewOnlyWallet(const FSN_WalletData &walletData, bool testnet) const;
    static FSN_WalletData walletData(Monero::Wallet * wallet);

    Monero::Wallet * getMyWalletByAddress(const std::string &address) const;

private:
    // directory where view-only wallets for other FSNs will be stored
    std::string                  m_fsnWalletsDir;
    cryptonote::BlockchainDB   * m_bdb     = nullptr;
    cryptonote::Blockchain     * m_bc      = nullptr;
    cryptonote::tx_memory_pool * m_mempool = nullptr;

    mutable Monero::Wallet *m_stakeWallet = nullptr;
    mutable Monero::Wallet *m_minerWallet = nullptr;
    mutable std::map<std::string, Monero::Wallet*> m_viewOnlyWallets;

};

} // namespace supernode

#endif /* FSN_SERVANT_H_ */
