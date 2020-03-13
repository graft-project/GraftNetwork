# Multistage docker build, requires docker 17.05

# TO RUN
# docker build -t loki-daemon-image .

# TO COLLECT BINARIES
# ./util/build_scripts/collect_from_docker_container.sh

# builder stage
FROM ubuntu:16.04 as builder

RUN set -ex && \
    apt-get update && \
    apt-get install -y curl apt-transport-https eatmydata && \
    echo 'deb https://apt.kitware.com/ubuntu/ xenial main' >/etc/apt/sources.list.d/kitware-cmake.list && \
    curl https://apt.kitware.com/keys/kitware-archive-latest.asc | apt-key add - && \
    apt-get update && \
    eatmydata apt-get --no-install-recommends --yes install \
        ca-certificates \
        cmake \
        g++ \
        make \
        pkg-config \
        graphviz \
        doxygen \
        git \
        libtool-bin \
        autoconf \
        automake \
        bzip2 \
        xsltproc \
        gperf

WORKDIR /usr/local/src

ARG OPENSSL_VERSION=1.1.1d
ARG OPENSSL_HASH=1e3a91bc1f9dfce01af26026f856e064eab4c8ee0a8f457b5ae30b40b8b711f2
RUN set -ex \
    && curl -s -O https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz \
    && echo "${OPENSSL_HASH}  openssl-${OPENSSL_VERSION}.tar.gz" | sha256sum -c \
    && tar xf openssl-${OPENSSL_VERSION}.tar.gz \
    && cd openssl-${OPENSSL_VERSION} \
    && ./Configure --prefix=/usr linux-x86_64 no-shared --static \
    && make -j$(nproc) \
    && make install_sw -j$(nproc)

ARG BOOST_VERSION=1_72_0
ARG BOOST_VERSION_DOT=1.72.0
ARG BOOST_HASH=59c9b274bc451cf91a9ba1dd2c7fdcaf5d60b1b3aa83f2c9fa143417cc660722
RUN set -ex \
    && curl -s -L -o  boost_${BOOST_VERSION}.tar.bz2 https://dl.bintray.com/boostorg/release/${BOOST_VERSION_DOT}/source/boost_${BOOST_VERSION}.tar.bz2 \
    && echo "${BOOST_HASH}  boost_${BOOST_VERSION}.tar.bz2" | sha256sum -c \
    && tar xf boost_${BOOST_VERSION}.tar.bz2 \
    && cd boost_${BOOST_VERSION} \
    && ./bootstrap.sh \
    && ./b2 --prefix=/usr --build-type=minimal link=static runtime-link=static \
        --with-atomic --with-chrono --with-date_time --with-filesystem --with-program_options \
        --with-regex --with-serialization --with-system --with-thread --with-locale \
        threading=multi threadapi=pthread cxxflags=-fPIC \
        -j$(nproc) install


ARG SODIUM_VERSION=1.0.18-RELEASE
ARG SODIUM_HASH=940ef42797baa0278df6b7fd9e67c7590f87744b
RUN set -ex \
    && git clone https://github.com/jedisct1/libsodium.git -b ${SODIUM_VERSION} --depth=1 \
    && cd libsodium \
    && test `git rev-parse HEAD` = ${SODIUM_HASH} || exit 1 \
    && ./autogen.sh \
    && ./configure --enable-static --disable-shared --prefix=/usr \
    && make -j$(nproc) \
    && make check \
    && make install

# Readline
# ARG READLINE_VERSION=8.0
# ARG READLINE_HASH=e339f51971478d369f8a053a330a190781acb9864cf4c541060f12078948e461
# RUN set -ex \
#     && curl -s -O https://ftp.gnu.org/gnu/readline/readline-${READLINE_VERSION}.tar.gz \
#     && echo "${READLINE_HASH}  readline-${READLINE_VERSION}.tar.gz" | sha256sum -c \
#     && tar xf readline-${READLINE_VERSION}.tar.gz \
#     && cd readline-${READLINE_VERSION} \
#     && ./configure --prefix=/usr --disable-shared \
#     && make -j$(nproc) \
#     && make install

# Sqlite3
ARG SQLITE_VERSION=3310100
ARG SQLITE_HASH=62284efebc05a76f909c580ffa5c008a7d22a1287285d68b7825a2b6b51949ae
RUN set -ex \
    && curl -s -O https://sqlite.org/2020/sqlite-autoconf-${SQLITE_VERSION}.tar.gz \
    && echo "${SQLITE_HASH}  sqlite-autoconf-${SQLITE_VERSION}.tar.gz" | sha256sum -c \
    && tar xf sqlite-autoconf-${SQLITE_VERSION}.tar.gz \
    && cd sqlite-autoconf-${SQLITE_VERSION} \
    && ./configure --disable-shared --prefix=/usr --with-pic \
    && make -j$(nproc) \
    && make install

WORKDIR /src
COPY . .

RUN set -ex && \
    git submodule update --init --recursive && \
    rm -rf build/release && mkdir -p build/release && cd build/release && \
    cmake -DSTATIC=ON -DARCH=x86-64 -DCMAKE_BUILD_TYPE=Release ../.. && \
    make -j$(nproc) VERBOSE=1

RUN set -ex && \
    ldd /src/build/release/bin/lokid

# runtime stage
FROM ubuntu:16.04

RUN set -ex && \
    apt-get update && \
    apt-get --no-install-recommends --yes install ca-certificates && \
    apt-get clean && \
    rm -rf /var/lib/apt
COPY --from=builder /src/build/release/bin /usr/local/bin/

# Create loki user
RUN adduser --system --group --disabled-password loki && \
	mkdir -p /wallet /home/loki/.loki && \
	chown -R loki:loki /home/loki/.loki && \
	chown -R loki:loki /wallet

# Contains the blockchain
VOLUME /home/loki/.loki

# Generate your wallet via accessing the container and run:
# cd /wallet
# loki-wallet-cli
VOLUME /wallet

EXPOSE 22022
EXPOSE 22023

# switch to user monero
USER loki

ENTRYPOINT ["lokid", "--p2p-bind-ip=0.0.0.0", "--p2p-bind-port=22022", "--rpc-bind-ip=0.0.0.0", "--rpc-bind-port=22023", "--non-interactive", "--confirm-external-bind"]
