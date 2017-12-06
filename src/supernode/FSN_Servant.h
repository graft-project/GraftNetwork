#ifndef FSN_SERVANT_H_
#define FSN_SERVANT_H_

#include "FSN_ServantBase.h"
#include <cryptonote_core/cryptonote_core.h>
#include <wallet/wallet2_api.h>
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

    FSN_Servant(const string &bdb_path, const string &daemon_addr, const string &fsn_wallets_dir, bool testnet = false);
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

    void AddFsnAccount(boost::shared_ptr<FSN_Data> fsn);
    void RemoveFsnAccount(boost::shared_ptr<FSN_Data> fsn);
    boost::shared_ptr<FSN_Data> FSN_DataByStakeAddr(const string& addr) const override;

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
    // next two fields may be references
    mutable boost::mutex All_FSN_Guard;// DO NOT block for long time. if need - use copy
    // TODO: store FSN_Data and corresponding wallet in single map
    vector< boost::shared_ptr<FSN_Data> > All_FSN;// access to this data may be done from different threads

    bool                         m_testnet = false;
    std::string                  m_daemonAddr;
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
