#!/bin/bash

DAEMON_ADDRESS="localhost:28281"
WALLET_PATH=$HOME/dev/graft-project/testnet_latest_local/wallet_m
WALLET_PASSWORD=

INPUT_DIR=.
OUTPUT_DIR=.

INPUT_FILE=generated_txs.txt
OUTPUT_FILE=received_txs.txt

./tx_tests monitor --daemon-addres=$DAEMON_ADDRESS --wallet-path=$WALLET_PATH  \
    --input-file=$INPUT_FILE \
    --output-file=$OUTPUT_FILE \
    --timeout=30


TS=`date +%Y%m%d%H%M%S`
cp $OUTPUT_FILE $OUTPUT_FILE"_"$TS






