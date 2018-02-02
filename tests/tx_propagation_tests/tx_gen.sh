#!/bin/bash

DAEMON_ADDRESS="localhost:28281"
WALLET_PATH=$HOME/dev/graft-project/testnet_latest_local/wallet_m
WALLET_PASSWORD=

INPUT_DIR=.
OUTPUT_DIR=.
INPUT_FILE=payments.txt

OUTPUT_FILE=generated_txs.txt

./tx_tests generate --daemon-addres=$DAEMON_ADDRESS --wallet-path=$WALLET_PATH  \
    --output-dir=$OUTPUT_DIR \
    --output-file=$OUTPUT_FILE  \
    --input-file=$INPUT_FILE




TS=`date +%Y%m%d%H%M%S`
cp $OUTPUT_FILE $OUTPUT_FILE"_"$TS

