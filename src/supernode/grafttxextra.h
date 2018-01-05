#ifndef GRAFTTXEXTRA_H
#define GRAFTTXEXTRA_H

#include "cryptonote_basic/account.h"

namespace supernode {

struct GraftTxExtra
{
    uint64_t BlockNum;
    std::string PaymentID;
    std::vector<std::string> Signs;

    GraftTxExtra() = default;
    GraftTxExtra(uint64_t _blocknum, const std::string &_payment_id, const std::vector<std::string> _signs);
    bool operator ==(const GraftTxExtra &rhs) const;


    BEGIN_SERIALIZE_OBJECT()
        FIELD(BlockNum)
        FIELD(PaymentID)
        FIELD(Signs)
    END_SERIALIZE()
};

}


#endif // GRAFTTXEXTRA_H
