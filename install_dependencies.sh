sudo apt-get update
sudo apt-get upgrade
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libboost-all-dev \
    libssl-dev \
    libunbound-dev \
    uuid-dev \
    libminiupnpc-dev \
    libunwind8-dev \
    liblzma-dev \
    libldns-dev \
    libexpat1-dev \
    doxygen \
    graphviz

sudo apt-get install libgtest-dev && cd /usr/src/gtest && sudo cmake . && sudo make && sudo mv libg* /usr/lib/
