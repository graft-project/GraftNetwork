#include "ptx_proxy.h"

namespace Monero {

PtxProxy::~PtxProxy() {}

PtxProxy * PtxProxy::deserialize(const std::string &ptx_hex)
{
    PtxProxyImpl * ptx = new PtxProxyImpl();
    if (!tools::wallet2::pending_tx::deserialize(ptx_hex, ptx->m_ptx)) {
        delete ptx;
        ptx = nullptr;
    }
    return ptx;
}


std::string PtxProxyImpl::serialize() const
{
    return tools::wallet2::pending_tx::serialize(m_ptx);
}

std::string PtxProxyImpl::txBlob() const
{
    return cryptonote::tx_to_blob(m_ptx.tx);
}

std::string PtxProxyImpl::txKeyBlob() const
{
    return std::string(reinterpret_cast<const char*>(&m_ptx.tx_key), sizeof(crypto::secret_key));
}

std::string PtxProxyImpl::txHash() const
{
    return epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(m_ptx.tx));
}

}

