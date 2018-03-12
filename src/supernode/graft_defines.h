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

#include <string>

#define ERROR_MESSAGE(message) std::string(__FUNCTION__) + std::string(": ") + message
#define EXTENDED_ERROR_MESSAGE(message, reason) \
    std::string(__FUNCTION__) + std::string(": ") + message + std::string(" Reason: ") + reason

//Standart JSON-RPC 2.0 Errors
#define ERROR_PARSE_ERROR                   -32700
#define ERROR_INVALID_REQUEST               -32600
#define ERROR_METHOD_NOT_FOUND              -32601
#define ERROR_INVALID_PARAMS                -32602
#define ERROR_INTERNAL_ERROR                -32603

//DAPI Format Errors
#define ERROR_WRONG_DAPI_URI                -32000
#define ERROR_NO_DAPI_VERSION               -32001
#define ERROR_WRONG_DAPI_VERSION            -32002
#define ERROR_HANDLER_NOT_FOUND             -32003
#define ERROR_UNKNOWN_METHOD_ERROR          -32004

//DAPI Wallet Errors
#define ERROR_OPEN_WALLET_FAILED            -32010
#define ERROR_CREATE_WALLET_FAILED          -32011
#define ERROR_RESTORE_WALLET_FAILED         -32012
#define ERROR_LANGUAGE_IS_NOT_FOUND         -32013
#define ERROR_ELECTRUM_SEED_EMPTY           -32014
#define ERROR_ELECTRUM_SEED_INVALID         -32015
#define ERROR_WALLET_RESTRICTED             -32016
#define ERROR_TRANSACTION_INVALID           -32017
#define ERROR_TRANSACTION_TO_LARGE          -32018
#define ERROR_TRANSFER_FAILED               -32019
#define ERROR_CRYPTONODE_BUSY               -32020
#define ERROR_BALANCE_NOT_AVAILABLE         -32030

const std::string MESSAGE_OPEN_WALLET_FAILED("Failed to open wallet.");
const std::string MESSAGE_CREATE_WALLET_FAILED("Failed to create wallet.");
const std::string MESSAGE_RESTORE_WALLET_FAILED("Failed to restore wallet.");
const std::string MESSAGE_LANGUAGE_IS_NOT_FOUND("The required language is not found.");
const std::string MESSAGE_ELECTRUM_SEED_EMPTY("Electrum seed is empty.");
const std::string MESSAGE_ELECTRUM_SEED_INVALID("Electrum seed is invalid.");
const std::string MESSAGE_WALLET_RESTRICTED("The wallet is restricted.");
const std::string MESSAGE_TRANSACTION_INVALID("The transaction is invalid.");
const std::string MESSAGE_TRANSACTION_TO_LARGE("The transaction is too large.");
const std::string MESSAGE_TRANSFER_FAILED("The transfer is failed.");
const std::string MESSAGE_CRYPTONODE_BUSY("Graft Node is busy.");
const std::string MESSAGE_BALANCE_NOT_AVAILABLE("Couldn't get the balance of wallet.");

//RTA DAPI Errors
#define ERROR_SALE_REQUEST_FAILED           -32050
#define ERROR_CANNOT_REJECT_PAY             -32060

const std::string MESSAGE_SALE_REQUEST_FAILED("Sale request is failed.");
const std::string MESSAGE_CANNOT_REJECT_PAY("Cannot reject pay.");

#endif // GRAFT_DEFINES_H
