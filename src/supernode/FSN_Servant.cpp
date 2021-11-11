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
// Parts of this file are originally copyright (c) 2014-2017 The Monero Project

#include "FSN_Servant.h"
#include <blockchain_db/blockchain_db.h>
#include <cryptonote_core/tx_pool.h>
#include <cryptonote_core/blockchain.h>
#include <crypto/crypto.h>
#include <exception>


using namespace cryptonote;
using namespace Monero;

static unsigned s_AuthSampleSize = 8;

namespace supernode {

namespace consts {
    static const string DEFAULT_FSN_WALLETS_DIR = "/tmp/graft/fsn_data/wallets_vo";
    static const int    DEFAULT_FSN_WALLET_REFRESH_INTERVAL_MS = 5000;
}

namespace helpers {
crypto::public_key get_tx_gen_pub_key(const transaction &tx)
{
    if (!is_coinbase(tx)) {
        return crypto::null_pkey;
    }
    const tx_out &out = tx.vout.at(0);
    return boost::get<txout_to_key>(out.target).key;
}


// copied code from graft-blockchain-explorer
// TODO: optimize it for the purpose
crypto::public_key get_tx_pub_key_from_received_outs(const transaction &tx)
{
    std::vector<tx_extra_field> tx_extra_fields;

    if(!parse_tx_extra(tx.extra, tx_extra_fields))
    {
        // Extra may only be partially parsed, it's OK if tx_extra_fields contains public key
    }

    // Due to a previous bug, there might be more than one tx pubkey in extra, one being
    // the result of a previously discarded signature.
    // For speed, since scanning for outputs is a slow process, we check whether extra
    // contains more than one pubkey. If not, the first one is returned. If yes, they're
    // checked for whether they yield at least one output
    tx_extra_pub_key pub_key_field;

    if (!find_tx_extra_field_by_type(tx_extra_fields, pub_key_field, 0))
    {
        return crypto::null_pkey;
    }

    crypto::public_key tx_pub_key = pub_key_field.pub_key;

    bool two_found = find_tx_extra_field_by_type(tx_extra_fields, pub_key_field, 1);

    if (!two_found)
    {
        // easy case, just one found
        return tx_pub_key;
    }
    else
    {
        // just return second one if there are two.
        // this does not require private view key, as
        // its not needed for my use case.
        return pub_key_field.pub_key;
    }

    return crypto::null_pkey;
}
} // namespace helpers



//FSN_Servant::FSN_Servant()
//{

//}

//FSN_Servant::FSN_Servant(const FSN_Servant &other)
//{

//}

FSN_Servant::FSN_Servant(const string &bdb_path, const string &node_addr, const string &node_login, const string &node_password,
                         const string &fsn_wallets_dir, network_type nettype)
    : m_fsnWalletsDir(fsn_wallets_dir)
    , m_mempool(m_bc)
    , m_snl(m_bc)
    , m_bc(m_mempool, m_snl)
{
    FSN_ServantBase::m_nettype      = nettype;
    FSN_ServantBase::m_nodelogin    = node_login;
    FSN_ServantBase::m_nodePassword = node_password;
    SetNodeAddress(node_addr);

    if (m_fsnWalletsDir.empty())
        m_fsnWalletsDir = consts::DEFAULT_FSN_WALLETS_DIR;

    // create directory for "view only wallets" if not exists
    if (!boost::filesystem::exists(m_fsnWalletsDir)) {
        if (!boost::filesystem::create_directories(m_fsnWalletsDir))
            throw std::runtime_error("Error creating FSN view only wallets directory");
    }
//FIXME: Commented since blockchain loading disabled.
//    if (!initBlockchain(bdb_path, nettype))
//        throw std::runtime_error("Failed to open blockchain");

}

void FSN_Servant::Set(const string& stakeFileName, const string& stakePasswd, const string& minerFileName, const string& minerPasswd)
{
    m_stakeWallet = initWallet(m_stakeWallet, stakeFileName, stakePasswd, m_nettype);
    m_minerWallet = initWallet(m_minerWallet, minerFileName, minerPasswd, m_nettype);
    m_stakeWallet->refresh();
    m_stakeWallet->store("");
    m_minerWallet->refresh();
    m_minerWallet->store("");
}


vector<pair<uint64_t, boost::shared_ptr<supernode::FSN_Data>>>
FSN_Servant::LastBlocksResolvedByFSN(uint64_t startFromBlock, uint64_t blockNums) const
{
    vector<pair<uint64_t, boost::shared_ptr<FSN_Data>>> result;
    uint64_t block_height = m_bc.get_current_blockchain_height();
    if (startFromBlock >= block_height) {
        LOG_ERROR("starting block is not in blockchain: " << startFromBlock);
        return result;
    }

    // option #2: as we have direct access to blockchain, we can use Blockchain class directly
    //1. retrieve the blocks

    LOG_PRINT_L3("Start checking from block " << startFromBlock);
    LOG_PRINT_L3("block height " << block_height);
    uint64_t endBlock = std::min(block_height - 1, startFromBlock + blockNums - 1);

    // TODO: read/write lock
    All_FSN_Guard.lock();

    for (uint64_t block_index = endBlock; block_index >= startFromBlock; --block_index) {
//FIXME: Commented since blockchain loading disabled.
//        const cryptonote::block block = m_bdb->get_block_from_height(block_index);
        //2. for each blocks, apply function from xmrblocks (page.h:show_my_outputs)
        // TODO: can be faster algorithm?
        for (const auto & fsn_wallet : All_FSN) {
            crypto::secret_key viewkey;
            epee::string_tools::hex_to_pod(fsn_wallet->Miner.ViewKey, viewkey);
            cryptonote::address_parse_info address_info;
            LOG_PRINT_L3("parsing address : " << fsn_wallet->Miner.Addr << "; testnet: " << m_nettype);

            if (!cryptonote::get_account_address_from_str(address_info, m_nettype, fsn_wallet->Miner.Addr)) {
                LOG_ERROR("Error parsing address: " << fsn_wallet->Miner.Addr);
                // throw exception here?
                continue;
            }

            LOG_PRINT_L3("pub spend key: " << epee::string_tools::pod_to_hex(address_info.address.m_spend_public_key));
            LOG_PRINT_L3("pub view key: " << epee::string_tools::pod_to_hex(address_info.address.m_view_public_key));
//FIXME: Commented since blockchain loading disabled.
//            if (proofCoinbaseTx(address_info.address, block, viewkey)) {
//                result.push_back(std::make_pair(block_index, fsn_wallet));
//                // stop wallets loop as we already found the wallet who solved block
//                break;
//            }
        }
    }
    All_FSN_Guard.unlock();

    return result;
}

vector< boost::shared_ptr<supernode::FSN_Data> > FSN_Servant::GetAuthSample(uint64_t forBlockNum) const
{
    return vector< boost::shared_ptr<supernode::FSN_Data> >();
}
uint64_t FSN_Servant::GetCurrentBlockHeight() const
{
    
    return m_bc.get_current_blockchain_height();
    
}


string FSN_Servant::SignByWalletPrivateKey(const string& str, const string& wallet_addr) const
{
    Monero::Wallet * wallet = getMyWalletByAddress(wallet_addr);
    if (!wallet)
        throw std::runtime_error("Address doesn't belong to any wallet of this supernode: " + wallet_addr);

    return wallet->signMessage(str);
}

bool FSN_Servant::IsSignValid(const string &message, const string &address, const string &signature) const
{
    // verify can be done by any wallet
    // TODO: make is static in wallet2_api/wallet2;
    return m_minerWallet->verifySignedMessage(message, address, signature);
}


uint64_t FSN_Servant::GetWalletBalance(uint64_t block_num, const FSN_WalletData& wallet) const
{
    // create or open view-only wallet;
    Monero::Wallet * w = initViewOnlyWallet(wallet, m_nettype);
    uint64_t result = w->unlockedBalance(block_num);

    return result;
}

void FSN_Servant::AddFsnAccount(boost::shared_ptr<FSN_Data> fsn) {
	FSN_ServantBase::AddFsnAccount(fsn);
    // create view-only wallet for stake account
    initViewOnlyWallet(fsn->Stake, m_nettype);
}

bool FSN_Servant::RemoveFsnAccount(boost::shared_ptr<FSN_Data> fsn) {
    boost::lock_guard<boost::recursive_mutex> lock(All_FSN_Guard);// we use one mutex for All_FSN && for m_viewOnlyWallets

    if( !FSN_ServantBase::RemoveFsnAccount(fsn) ) return false;

    // TODO: RAII based (scoped) locks
    const auto &it = m_viewOnlyWallets.find(fsn->Stake.Addr);
    if (it != m_viewOnlyWallets.end()) {
        Monero::Wallet * w = it->second;
        Monero::WalletManagerFactory::getWalletManager()->closeWallet(w);
        m_viewOnlyWallets.erase(it);
    } else {
        LOG_ERROR("Internal error: All_FSN doesn't have corresponding wallet: " << fsn->Stake.Addr);
    }

    return true;

}

FSN_WalletData FSN_Servant::GetMyStakeWallet() const
{
    if (!m_stakeWallet)
        throw std::runtime_error("Stake wallet non initialized");

    return walletData(m_stakeWallet);
}

FSN_WalletData FSN_Servant::GetMyMinerWallet() const
{
    if (!m_minerWallet)
        throw std::runtime_error("Miner wallet non initialized");

    return walletData(m_minerWallet);
}

bool FSN_Servant::proofCoinbaseTx(const cryptonote::account_public_address &address, const cryptonote::block &block,
                                  const crypto::secret_key &viewkey)
{
    // public transaction key is combined with our viewkey
    // to create, so called, derived key.
    // TODO: check why block.hash is invalid here;
    // LOG_PRINT_L3("checking block: " << epee::string_tools::pod_to_hex(block.hash) << ", hash valid: " << block.is_hash_valid());

    crypto::public_key tx_pubkey = helpers::get_tx_pub_key_from_received_outs(block.miner_tx);

    crypto::key_derivation derivation;
    if (!generate_key_derivation(tx_pubkey, viewkey, derivation)) {
        LOG_ERROR("Cant get derived key for: "  << "\n"
             << "pub_view_key: " << tx_pubkey << " and "
             << "prv_view_key" << viewkey);
        return false;
    }

    LOG_PRINT_L3("view_pub_key: " << epee::string_tools::pod_to_hex(address.m_view_public_key));
    LOG_PRINT_L3("priv_viewkey: " << epee::string_tools::pod_to_hex(viewkey));
    // TODO: check why tx_id is invalid here
    LOG_PRINT_L3("tx_id: " << epee::string_tools::pod_to_hex(block.miner_tx.hash));
    crypto::public_key output_pubkey = helpers::get_tx_gen_pub_key(block.miner_tx);
    crypto::public_key tx_pubkey_derived;
    crypto::derive_public_key(derivation,
                      0,
                      address.m_spend_public_key,
                      tx_pubkey_derived);

    LOG_PRINT_L3("out pubkey: " << epee::string_tools::pod_to_hex(output_pubkey));
    LOG_PRINT_L3("pubkey derived: " << epee::string_tools::pod_to_hex(tx_pubkey_derived));

    return tx_pubkey_derived == output_pubkey;
}

bool FSN_Servant::initBlockchain(const string &dbpath, network_type nettype)
{
    

    m_bdb = cryptonote::new_db("lmdb");
    if (!m_bdb) {
        LOG_ERROR("Error initializing blockchain db");
        // TODO: set status
        return false;
    }

    boost::filesystem::path folder(dbpath);
    folder = boost::filesystem::canonical(folder);

    folder /= m_bdb->get_db_name();
    const std::string filename = folder.string();
    LOG_PRINT_L0("Loading blockchain from folder " << filename << " ...");
    try
    {
        m_bdb->open(filename, nettype, DBF_RDONLY);
    }
    catch (const std::exception& e)
    {
        LOG_PRINT_L0("Error opening database: " << e.what());
        return false;
    }
    bool result = m_bc.init(m_bdb, nullptr, nettype);
    CHECK_AND_ASSERT_MES(result, false, "Failed to initialize source blockchain storage");
    LOG_PRINT_L0("Source blockchain storage initialized OK");
    return result;
}

Wallet *FSN_Servant::initWallet(Wallet * existingWallet, const string &path, const string &password, network_type nettype)
{
    WalletManagerBase * wmgr = Monero::WalletManagerFactory::getWalletManager();

    if (existingWallet)
        wmgr->closeWallet(existingWallet);
    // TODO: wallet2_api still not updated with network type
    Wallet * wallet = wmgr->openWallet(path, password, static_cast<Monero::NetworkType>(nettype));

    // we couldn't open wallet, delete the wallet object and throw exception
    if (wallet->status() != Wallet::Status_Ok) {
        string error_msg = wallet->errorString();
        // in case closeWallet failure, wallet object wont be deleted by 'closeWallet'
        if (!wmgr->closeWallet(wallet))
            delete wallet;
        throw runtime_error(string("error opening wallet: ") + error_msg);
    }

    if (!wallet->init(GetNodeAddress(), 0)) {
        MERROR("Can't connect to a daemon.");
    }

    return wallet;
}

Wallet *FSN_Servant::initViewOnlyWallet(const FSN_WalletData &walletData, network_type nettype) const
{

    if (walletData.Addr.empty()) {
        LOG_ERROR("Adding wallet with empty address");
        return nullptr;
    }

    const auto & walletIter = m_viewOnlyWallets.find(walletData.Addr);
    if (walletIter != m_viewOnlyWallets.end())
        return walletIter->second;

    // No luck, somehow caller requested the address we don't have view-only wallet yet
    boost::filesystem::path wallet_path (m_fsnWalletsDir);
    wallet_path /= walletData.Addr;
    Monero::Wallet * w = nullptr;
    Monero::WalletManagerBase * wmgr = Monero::WalletManagerFactory::getWalletManager();

    if (!wmgr->walletExists(wallet_path.string())) {
        // create new view only wallet
        w = wmgr->createWalletFromKeys(wallet_path.string(), "English",
                                       static_cast<Monero::NetworkType>(nettype), 0, walletData.Addr, walletData.ViewKey);
    } else {
        // open existing
        w = wmgr->openWallet(wallet_path.string(), "", static_cast<Monero::NetworkType>(nettype));
    }

    if (!w)
        throw std::runtime_error(std::string("unable to open/create view only wallet: " + wmgr->errorString()));


    if (!w->init(GetNodeAddress(), 0)) {
        MERROR("Can't connect to a daemon.");
    } else {
        w->setAutoRefreshInterval(consts::DEFAULT_FSN_WALLET_REFRESH_INTERVAL_MS);
        w->startRefresh();
        w->refresh();
    }

    // add wallet to map
    m_viewOnlyWallets[walletData.Addr] = w;
    // TODO: should have opened wallets in sync with the All_FSN ???
    return w;
}



FSN_WalletData FSN_Servant::walletData(Wallet *wallet)
{
    FSN_WalletData result = FSN_WalletData(wallet->address(), wallet->secretViewKey());
    return result;
}

Wallet *FSN_Servant::getMyWalletByAddress(const string &address) const
{
    Monero::Wallet * wallet = nullptr;
    if (address == GetMyMinerWallet().Addr) {
        wallet = m_minerWallet;
    } else if (address == GetMyStakeWallet().Addr) {
        wallet = m_stakeWallet;
    }

    return wallet;
}



FSN_ServantBase::~FSN_ServantBase() {}

unsigned FSN_Servant::AuthSampleSize() const { return s_AuthSampleSize; }

} // namespace supernode
