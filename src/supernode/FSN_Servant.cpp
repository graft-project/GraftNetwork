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
#include <exception>

using namespace cryptonote;
using namespace Monero;

static const unsigned s_uAuthSampleSize = 8;

namespace supernode {

namespace helpers {
public_key get_tx_gen_pub_key(const transaction &tx)
{
    if (!is_coinbase(tx)) {
        return null_pkey;
    }
    const tx_out &out = tx.vout.at(0);
    return boost::get<txout_to_key>(out.target).key;
}


// copied code from monero-blockchain-explorer
// TODO: optimize it for the purpose
public_key get_tx_pub_key_from_received_outs(const transaction &tx)
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
        return null_pkey;
    }

    public_key tx_pub_key = pub_key_field.pub_key;

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

    return null_pkey;
}
} // namespace helpers

FSN_Servant::FSN_Servant()
{

}

FSN_Servant::FSN_Servant(const FSN_Servant &other)
{

}

FSN_Servant::FSN_Servant(const string &bdb_path, const string &daemon_addr, bool testnet)
    : m_testnet{testnet},
      m_daemonAddr{daemon_addr}
{

    if (!initBlockchain(bdb_path, testnet))
        throw std::runtime_error("Failed to open blockchain");

}

void FSN_Servant::Set(const string& stakeFileName, const string& stakePasswd, const string& minerFileName, const string& minerPasswd)
{
    m_stakeWallet = initWallet(m_stakeWallet, stakeFileName, stakePasswd, m_testnet);
    m_minerWallet = initWallet(m_minerWallet, minerFileName, minerPasswd, m_testnet);
}


vector<pair<uint64_t, boost::shared_ptr<supernode::FSN_Data>>>
FSN_Servant::LastBlocksResolvedByFSN(uint64_t startFromBlock, uint64_t blockNums) const
{
    vector<pair<uint64_t, boost::shared_ptr<FSN_Data>>> result;
    uint64_t block_height = m_bc->get_current_blockchain_height();
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
        const cryptonote::block block = m_bdb->get_block_from_height(block_index);
        //2. for each blocks, apply function from xmrblocks (page.h:show_my_outputs)
        // TODO: can be faster algorithm?
        for (const auto & fsn_wallet : All_FSN) {
            secret_key viewkey;
            epee::string_tools::hex_to_pod(fsn_wallet->Miner.ViewKey, viewkey);
            cryptonote::account_public_address address;
            LOG_PRINT_L3("parsing address : " << fsn_wallet->Miner.Addr << "; testnet: " << m_testnet);

            if (!cryptonote::get_account_address_from_str(address, m_testnet, fsn_wallet->Miner.Addr)) {
                LOG_ERROR("Error parsing address: " << fsn_wallet->Miner.Addr);
                // throw exception here?
                continue;
            }

            LOG_PRINT_L3("pub spend key: " << epee::string_tools::pod_to_hex(address.m_spend_public_key));
            LOG_PRINT_L3("pub view key: " << epee::string_tools::pod_to_hex(address.m_view_public_key));

            if (proofCoinbaseTx(address, block, viewkey)) {
                result.push_back(std::make_pair(block_index, fsn_wallet));
                // stop wallets loop as we already found the wallet who solved block
                break;
            }
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
    return m_bc->get_current_blockchain_height();
}


string FSN_Servant::SignByWalletPrivateKey(const string& str, const string& wallet_addr) const
{
    Monero::Wallet * wallet = walletByAddress(wallet_addr);
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
    return 0;
}

void FSN_Servant::AddFsnAccount(boost::shared_ptr<FSN_Data> fsn)
{
    All_FSN_Guard.lock();
    All_FSN.push_back(fsn);
    All_FSN_Guard.unlock();
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
    public_key output_pubkey = helpers::get_tx_gen_pub_key(block.miner_tx);
    public_key tx_pubkey_derived;
    derive_public_key(derivation,
                      0,
                      address.m_spend_public_key,
                      tx_pubkey_derived);

    LOG_PRINT_L3("out pubkey: " << epee::string_tools::pod_to_hex(output_pubkey));
    LOG_PRINT_L3("pubkey derived: " << epee::string_tools::pod_to_hex(tx_pubkey_derived));

    return tx_pubkey_derived == output_pubkey;
}

bool FSN_Servant::initBlockchain(const string &dbpath, bool testnet)
{

    m_bc = nullptr;
    m_mempool = new cryptonote::tx_memory_pool(*m_bc);
    m_bc = new cryptonote::Blockchain(*m_mempool);

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
        m_bdb->open(filename, DBF_RDONLY);
    }
    catch (const std::exception& e)
    {
        LOG_PRINT_L0("Error opening database: " << e.what());
        return false;
    }
    bool result = m_bc->init(m_bdb, testnet);
    CHECK_AND_ASSERT_MES(result, false, "Failed to initialize source blockchain storage");
    LOG_PRINT_L0("Source blockchain storage initialized OK");
    return result;
}

Wallet *FSN_Servant::initWallet(Wallet * existingWallet, const string &path, const string &password, bool testnet)
{
    WalletManager * wmgr = Monero::WalletManagerFactory::getWalletManager();

    if (existingWallet)
        wmgr->closeWallet(existingWallet);
    Wallet * wallet = wmgr->openWallet(path, password, testnet);
    if (!wallet)
        throw runtime_error(string("error opening wallet: ") + wmgr->errorString());

    if (!wallet->init(m_daemonAddr, 0)) {
        MERROR("Can't connect to a daemon.");
    }
    return wallet;
}



FSN_WalletData FSN_Servant::walletData(Wallet *wallet)
{
    FSN_WalletData result = FSN_WalletData(wallet->address(), wallet->secretViewKey());
    return result;
}

Wallet *FSN_Servant::walletByAddress(const string &address) const
{
    Monero::Wallet * wallet = nullptr;
    if (address == GetMyMinerWallet().Addr) {
        wallet = m_minerWallet;
    } else if (address == GetMyStakeWallet().Addr) {
        wallet = m_stakeWallet;
    }

    return wallet;
}

unsigned FSN_Servant::AuthSampleSize() const { return s_uAuthSampleSize; }

FSN_ServantBase::~FSN_ServantBase() {}

} // namespace supernode
