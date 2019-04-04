#!/bin/bash

VERSTR="$( lsb_release -r )"
VER=($VERSTR)

sudo apt-get update
sudo apt-get upgrade
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libboost-all-dev \
    libunbound-dev \
    libminiupnpc-dev \
    libunwind8-dev \
    liblzma-dev \
    libldns-dev \
    libexpat1-dev \
    doxygen \
    graphviz

if [ ${VER[1]} == "18.04" ]; then
    sudo apt-get install -y libssl1.0-dev
else
    sudo apt-get install -y libssl-dev
fi

sudo apt-get install libgtest-dev && cd /usr/src/gtest && sudo cmake . && sudo make && sudo mv libg* /usr/lib/
