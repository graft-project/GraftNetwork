#ifndef GRAFT_DEFINES_H
#define GRAFT_DEFINES_H

#define STATUS_APPROVED     0
#define STATUS_PROCESSING   1
#define STATUS_REJECTED     2
#define STATUS_NONE         -1

#define STATUS_OK                           0
#define ERROR_PAYMENT_ID_DOES_NOT_EXISTS    -1
#define ERROR_PAYMENT_ID_ALREADY_EXISTS     -2
#define ERROR_EMPTY_PARAMS                  -3
#define ERROR_ACCOUNT_LOCKED                -4
#define ERROR_BROADCAST_FAILED              -5
#define ERROR_INVALID_TRANSACTION           -6
#define ERROR_ZERO_PAYMENT_AMOUNT           -7

#define ERROR_SALE_REQUEST_FAILED           -9
#define ERROR_LANGUAGE_IS_NOT_FOUND         -10
#define ERROR_CREATE_WALLET_FAILED          -11
#define ERROR_OPEN_WALLET_FAILED            -12
#define ERROR_RESTORE_WALLET_FAILED         -13
#define ERROR_ELECTRUM_SEED_EMPTY           -14
#define ERROR_ELECTRUM_SEED_INVALID         -15
#define ERROR_BALANCE_NOT_AVAILABLE         -16
#define ERROR_CANNOT_REJECT_PAY             -17


#endif // GRAFT_DEFINES_H
