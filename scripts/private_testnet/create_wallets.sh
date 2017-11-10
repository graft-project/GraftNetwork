#!/bin/bash

RPC_PORT=28281

function create_wallet {
    wallet_name=$1
    echo 1 | graft-wallet-cli  --testnet --trusted-daemon --daemon-address 127.0.0.1:$RPC_PORT --generate-new-wallet $wallet_name --password "" --restore-height=1
}



create_wallet wallet_01.bin
# create_wallet wallet_02.bin
# create_wallet wallet_03.bin
# create_wallet wallet_04.bin
# create_wallet wallet_05.bin
# create_wallet wallet_06.bin

create_wallet wallet_m


