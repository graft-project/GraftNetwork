#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REMOTE_HOST=graft-node1

pushd $DIR

cat sent_txs.txt_* > sent_txs.csv
ssh $REMOTE_HOST "cat ~/testnet_pub_test_tx_speed/received_txs.txt_* > ~/testnet_pub_test_tx_speed/received_txs.csv;"
scp $REMOTE_HOST:/home/ubuntu/testnet_pub_test_tx_speed/received_txs.csv .

popd



