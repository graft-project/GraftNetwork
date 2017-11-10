#!/bin/bash

RPC_PORT=28281
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function create_wallet {
    wallet_name=$1
    echo 1 | $DIR/graft-wallet-cli  --testnet --trusted-daemon --daemon-address 127.0.0.1:$RPC_PORT --generate-new-wallet $wallet_name --password "" --restore-height=1
}



create_wallet wallet_01
# create_wallet wallet_02
# create_wallet wallet_03
# create_wallet wallet_04
# create_wallet wallet_05
# create_wallet wallet_06

create_wallet wallet_m


