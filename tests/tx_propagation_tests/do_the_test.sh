#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

pushd $DIR

for i in {1..100}; do
    ./tx_gen.sh
    ./tx_send.sh
    sleep 1
done;

./get_results.sh


popd



