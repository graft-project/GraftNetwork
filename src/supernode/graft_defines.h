#ifndef GRAFT_DEFINES_H
#define GRAFT_DEFINES_H

#define STATUS_OK           0
#define STATUS_APPROVED     0
#define STATUS_PROCESSING   1
#define STATUS_REJECTED     2
#define STATUS_NONE         -1

#define ERROR_PAYMENT_ID_DOES_NOT_EXISTS    -1
#define ERROR_PAYMENT_ID_ALREADY_EXISTS     -2
#define ERROR_EMPTY_PARAMS                  -3
#define ERROR_ACCOUNT_LOCKED                -4
#define ERROR_BROADCAST_FAILED              -5
#define ERROR_INVALID_TRANSACTION           -6
#define ERROR_ZERO_PAYMENT_AMOUNT           -7

#endif // GRAFT_DEFINES_H
