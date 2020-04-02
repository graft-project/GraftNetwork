#ifndef PTXPROXY_IMPL_H
#define PTXPROXY_IMPL_H

#include "wallet2_api.h"
#include "wallet/wallet2.h"

namespace Monero {

class PtxProxyImpl : public PtxProxy
{
public:
    PtxProxyImpl() = default;
    ~PtxProxyImpl() override = default;
    std::string serialize() const override;
    std::string txBlob() const override;
    std::string txKeyBlob() const override;
    
    
private:
    tools::wallet2::pending_tx m_ptx;
};

} // namespace Monero

#endif // PTXPROXY_IMPL_H
