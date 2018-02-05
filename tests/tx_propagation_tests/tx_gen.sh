#!/bin/bash
# this script generates transaction and copies record to file with timestamp suffix

DAEMON_ADDRESS="localhost:28981"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WALLET_PATH=$DIR/wallet_m
WALLET_PASSWORD=

INPUT_DIR=.
OUTPUT_DIR=.
INPUT_FILE=payments.txt

OUTPUT_FILE=generated_txs.txt
REMOTE_HOST=graft-node1

pushd $DIR


./tx_tests generate --daemon-addres=$DAEMON_ADDRESS --wallet-path=$WALLET_PATH  \
    --output-dir=$OUTPUT_DIR \
    --output-file=$OUTPUT_FILE  \
    --input-file=$INPUT_FILE


TS=`date +%Y%m%d%H%M%S`
cp $OUTPUT_FILE $OUTPUT_FILE"_"$TS

scp $OUTPUT_FILE $REMOTE_HOST:/home/ubuntu/testnet_pub_test_tx_speed


popd
