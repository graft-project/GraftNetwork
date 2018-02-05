#!/bin/bash
# this script starts monitoring on remote host and sends transaction to a pool

DAEMON_ADDRESS="localhost:28981"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WALLET_PATH=$DIR/wallet_m
WALLET_PASSWORD=
REMOTE_HOST=graft-node1

INPUT_DIR=.
OUTPUT_DIR=.
OUTPUT_FILE=sent_txs.txt

pushd $DIR
ssh $REMOTE_HOST "~/testnet_pub_test_tx_speed/tx_mon.sh" &
sleep 3

./tx_tests send --daemon-addres=$DAEMON_ADDRESS --wallet-path=$WALLET_PATH  \
    --input-dir=$OUTPUT_DIR \
    --output-file=$OUTPUT_FILE

TS=`date +%Y%m%d%H%M%S`
cp $OUTPUT_FILE $OUTPUT_FILE"_"$TS

popd



