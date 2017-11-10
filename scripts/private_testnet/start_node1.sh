#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# graftnoned expected to be copied to this directory

$DIR/graftnoded --testnet --testnet-p2p-bind-port 28280 \
    --rpc-bind-ip 0.0.0.0 --testnet-rpc-bind-port 28281 \
    --no-igd --hide-my-port  --log-level 1 \
    --testnet-data-dir $DIR/node_01 \
    --p2p-bind-ip 127.0.0.1 \
    --add-exclusive-node 127.0.0.1:38280 \
    --add-exclusive-node 127.0.0.1:48280 \
    --confirm-external-bind



