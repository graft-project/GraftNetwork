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

#define ERROR_NOT_ENOUGH_COINS              -20


#endif // GRAFT_DEFINES_H
