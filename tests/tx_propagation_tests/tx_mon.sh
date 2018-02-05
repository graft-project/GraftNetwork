#!/bin/bash
# this script starts monitoring for transaction listed in '--input-file' argument
# finishes if transaction found in pool or timeout triggered

DAEMON_ADDRESS="localhost:28981"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WALLET_PATH=$DIR/wallet_m
WALLET_PASSWORD=

INPUT_DIR=.
OUTPUT_DIR=.

INPUT_FILE=generated_txs.txt
OUTPUT_FILE=received_txs.txt

pushd $DIR
./tx_tests monitor --daemon-addres=$DAEMON_ADDRESS --wallet-path=$WALLET_PATH  \
    --input-file=$INPUT_FILE \
    --output-file=$OUTPUT_FILE \
    --timeout=30


TS=`date +%Y%m%d%H%M%S`
cp $OUTPUT_FILE $OUTPUT_FILE"_"$TS


popd 




