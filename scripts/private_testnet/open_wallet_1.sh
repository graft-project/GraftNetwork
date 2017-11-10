#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
RPC_PORT=28281


rlwrap $DIR/graft-wallet-cli --wallet-file wallet_01.bin --password "" --testnet --trusted-daemon --daemon-address localhost:$RPC_PORT  --log-file wallet_01.log

