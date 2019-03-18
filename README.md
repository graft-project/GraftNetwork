# Graft Network

Copyright (c) 2019, The Graft Project

## Monero Fork (Monero version 0.11.1.0)

Copyright (c) 2014-2018, The Monero Project

Portions Copyright (c) 2012-2013, The Cryptonote developers

## Development Resources

- Web: [graft.network](https://www.graft.network)
- GitHub: [https://github.com/graft-project/GraftNetwork](https://github.com/graft-project/GraftNetwork)
- Monero GitHub: [https://github.com/monero-project/monero](https://github.com/monero-project/monero)

## Build

| Operating System      | Processor | Status |
| --------------------- | -------- |--------|

## Coverage

| Type      | Status |
|-----------|--------|
| License   | [![License](https://img.shields.io/badge/license-BSD3-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)

## Introduction

Graft Network is a private, secure, untraceable, decentralized digital cryptocurrency. You are your bank. You control your funds and nobody can trace your transactions unless you allow them to do so.

**Privacy:** Graft Network uses a cryptographically sound system to allow you to send and receive funds without your transactions being easily revealed on the blockchain (the ledger of transactions that everyone has access to). This ensures that your purchases, receipts, and all transfers remain absolutely private by default.

**Security:** Using the power of a distributed peer-to-peer consensus network, every transaction on the network is cryptographically secured. Individual wallets have a 25-word mnemonic seed that is only displayed once, and can be written down to backup the wallet. Wallet files are encrypted with a passphrase to ensure they are useless if stolen.

**Untraceability:** By taking advantage of ring signatures, a special property of a certain type of cryptography, Graft Network is able to ensure that transactions are not only untraceable, but have an optional measure of ambiguity that ensures that transactions cannot easily be tied back to an individual user or computer.

## About this Project

This is the core implementation of Graft Network. It is open source and completely free to use without restrictions, except for those specified in the license agreement below. There are no restrictions on anyone creating an alternative implementation of Graft Network that uses the protocol and network in a compatible manner.

As with many development projects, the repository on Github is considered to be the "staging" area for the latest changes. Before changes are merged into that branch on the main repository, they are tested by individual developers in their own branches, submitted as a pull request, and then subsequently tested by contributors who focus on testing and code reviews. Having said that, the repository should be carefully considered before using it in a production environment, unless there is a patch in the repository for a particular show-stopping issue you are experiencing. It is generally a better idea to use a tagged release for stability.

**Anyone is welcome to contribute to Graft Network's codebase!** If you have a fix or code change, feel free to submit it as a pull request directly to the "master" branch. In cases where the change is relatively small or does not affect other parts of the codebase, it may be merged in immediately by any one of the collaborators. On the other hand, if the change is particularly large or complex, it is expected that it will be discussed at length either well in advance of the pull request being submitted, or even directly on the pull request.

## Supporting the Project

Graft Network development can be supported directly through donations.

## License

See [LICENSE](LICENSE).

# Contributing

If you want to help out, see [CONTRIBUTING](CONTRIBUTING.md) for a set of guidelines.

## Vulnerability Response Process

See [Vulnerability Response Process](VULNERABILITY_RESPONSE_PROCESS.md).

## Graft Network software updates and consensus protocol changes (hard fork schedule)

Graft Network uses a fixed-schedule hard fork mechanism to implement new features. This means that users of Graft Network (end users and service providers) need to run current versions and update their software on a regular schedule. Here is the current schedule, versions, and compatibility.
Dates are provided in the format YYYY-MM-DD. 


| Fork Date         | Consensus version | Minimum Graft Network Version | Recommended Graft Network Version | Details            |
| ----------------- | ----------------- | ---------------------- | -------------------------- | ------------------ |
| 2018-01-18        | v7                | 1.0.0                | 1.0.1                    | First release      |
| 2018-04-10        | v8                | 1.1.0                | 1.1.2                    | Anti-ASIC change from Monero (Cryptonight variant 1), Improved Difficulty Adjustment Algorithm (new algorithm based on the LWMA difficulty algorithm)      |
| 2018-04-23        | v9                | 1.2.0                | 1.2.3                    | Fix for Difficulty Adjustment Algorithm      |
| 2018-09-17        | v10               | 1.4.4                | 1.4.5                    | Block reward halved       |
| 2018-10-31        | v11               | 1.5.0                | 1.5.1                    | PoW algorithm from Monero v8 (CN v2), enabled checkpoints for mainnet  |
| 2019-03-07        | v12               | 1.6.0                | 1.6.0                    | Own PoW algorithm - CryptoNight V8 ReverseWaltz - tweaked from CryptoNight Monero v8 (CN v2)  |
| 2019-03-18        | v13               | 1.7.1                | 1.7.1                    | RTA Mining  |

## Installing Graft Network from a Package

Not available yet!

## Compiling Graft Network from Source

### Dependencies

**Due to gcc 7.3.0 being a hard requirement right now, we strongly recomend to use Ubuntu 18.04 as a build platform**

The following table summarizes the tools and libraries required to build.  A
few of the libraries are also included in this repository (marked as
"Vendored"). By default, the build uses the library installed on the system,
and ignores the vendored sources. However, if no library is found installed on
the system, then the vendored source will be built and used. The vendored
sources are also used for statically-linked builds because distribution
packages often include only shared library binaries (`.so`) but not static
library archives (`.a`).

| Dep            | Min. Version  | Vendored | Debian/Ubuntu Pkg  | Arch Pkg       | Optional | Purpose        |
| -------------- | ------------- | ---------| ------------------ | -------------- | -------- | -------------- |
| GCC            | 7.3.0         | NO       | `build-essential`  | `base-devel`   | NO       |                |
| CMake          | 3.0.0         | NO       | `cmake`            | `cmake`        | NO       |                |
| pkg-config     | any           | NO       | `pkg-config`       | `base-devel`   | NO       |                |
| Boost          | 1.58          | NO       | `libboost-all-dev` | `boost`        | NO       | C++ libraries  |
| OpenSSL        | 1.0.2         | NO       | `libssl-dev`       | `openssl`      | NO       | sha256 sum     |
| libunbound     | 1.4.16        | YES      | `libunbound-dev`   | `unbound`      | NO       | DNS resolver   |
| libminiupnpc   | 2.0           | YES      | `libminiupnpc-dev` | `miniupnpc`    | YES      | NAT punching   |
| libunwind      | any           | NO       | `libunwind8-dev`   | `libunwind`    | YES      | Stack traces   |
| liblzma        | any           | NO       | `liblzma-dev`      | `xz`           | YES      | For libunwind  |
| ldns           | 1.6.17        | NO       | `libldns-dev`      | `ldns`         | YES      | SSL toolkit    |
| expat          | 1.1           | NO       | `libexpat1-dev`    | `expat`        | YES      | XML parsing    |
| GTest          | 1.5           | YES      | `libgtest-dev`^    | `gtest`        | YES      | Test suite     |
| Doxygen        | any           | NO       | `doxygen`          | `doxygen`      | YES      | Documentation  |
| Graphviz       | any           | NO       | `graphviz`         | `graphviz`     | YES      | Documentation  |

[^] On Debian/Ubuntu `libgtest-dev` only includes sources and headers. You must
build the library binary manually. This can be done with the following command ```sudo apt-get install libgtest-dev && cd /usr/src/gtest && sudo cmake . && sudo make && sudo mv libg* /usr/lib/ ```


### Cloning the repository
Clone recursively to pull-in needed submodule(s):

         git clone --recursive https://github.com/graft-project/GraftNetwork

If you already have a repo cloned, initialize and update:

         cd GraftNetwork && git submodule init && git submodule update



### Build instructions

Graft Network uses the CMake build system and a top-level [Makefile](Makefile) that
invokes cmake commands as needed.

#### On Linux and OS X

* Install the dependencies
* Change to the root of the source code directory and build:

         cd GraftNetwork
         make

    *Optional*: If your machine has several cores and enough memory, enable
    parallel build by running `make -j<number of threads>` instead of `make`. For
    this to be worthwhile, the machine should have one core and about 2GB of RAM
    available per thread.

* The resulting executables can be found in `build/release/bin`

* Add `PATH="$PATH:$HOME/GraftNetwork/build/release/bin"` to `.profile`

* Run Graft Network with `graftnoded --detach`

* **Optional**: build and run the test suite to verify the binaries:

        make release-test

    *NOTE*: `coretests` test may take a few hours to complete.

* **Optional**: to build binaries suitable for debugging:

         make debug

* **Optional**: to build statically-linked binaries:

         make release-static

* **Optional**: build documentation in `doc/html` (omit `HAVE_DOT=YES` if `graphviz` is not installed):

        HAVE_DOT=YES doxygen Doxyfile

#### On the Raspberry Pi 2

Tested on a Raspberry Pi 2 with a clean install of minimal Debian Jessie from https://www.raspberrypi.org/downloads/raspbian/

* `apt-get update && apt-get upgrade` to install all of the latest software

* Install the dependencies for Graft Network except libunwind and libboost-all-dev

* Increase the system swap size:
```	
	sudo /etc/init.d/dphys-swapfile stop  
	sudo nano /etc/dphys-swapfile  
	CONF_SWAPSIZE=1024  
	sudo /etc/init.d/dphys-swapfile start  
```
* Install the latest version of boost (this may first require invoking `apt-get remove --purge libboost*` to remove a previous version if you're not using a clean install):
```
	cd  
	wget https://sourceforge.net/projects/boost/files/boost/1.64.0/boost_1_64_0.tar.bz2  
	tar xvfo boost_1_64_0.tar.bz2  
	cd boost_1_64_0  
	./bootstrap.sh  
	sudo ./b2  
```
* Wait ~8 hours
```
	sudo ./bjam install
```
* Wait ~4 hours

* Change to the root of the source code directory and build:
```
        cd GraftNetwork
        make release
```
* Wait ~4 hours

* The resulting executables can be found in `build/release/bin`

* Add `PATH="$PATH:$HOME/GraftNetwork/build/release/bin"` to `.profile`

* Run Graft Network with `graftnoded --detach`

* You may wish to reduce the size of the swap file after the build has finished, and delete the boost directory from your home directory

#### On Windows:

Binaries for Windows are built on Windows using the MinGW toolchain within
[MSYS2 environment](http://msys2.github.io). The MSYS2 environment emulates a
POSIX system. The toolchain runs within the environment and *cross-compiles*
binaries that can run outside of the environment as a regular Windows
application.

**Preparing the Build Environment**

* Download and install the [MSYS2 installer](http://msys2.github.io), either the 64-bit or the 32-bit package, depending on your system.
* Open the MSYS shell via the `MSYS2 Shell` shortcut
* Update packages using pacman:  

        pacman -Syuu  

* Exit the MSYS shell using Alt+F4  
* Edit the properties for the `MSYS2 Shell` shortcut changing "msys2_shell.bat" to "msys2_shell.cmd -mingw64" for 64-bit builds or "msys2_shell.cmd -mingw32" for 32-bit builds
* Restart MSYS shell via modified shortcut and update packages again using pacman:  

        pacman -Syuu  


* Install dependencies:

    To build for 64-bit Windows:

        pacman -S mingw-w64-x86_64-toolchain make mingw-w64-x86_64-cmake mingw-w64-x86_64-boost

    To build for 32-bit Windows:
 
        pacman -S mingw-w64-i686-toolchain make mingw-w64-i686-cmake mingw-w64-i686-boost

* Open the MingW shell via `MinGW-w64-Win64 Shell` shortcut on 64-bit Windows
  or `MinGW-w64-Win64 Shell` shortcut on 32-bit Windows. Note that if you are
  running 64-bit Windows, you will have both 64-bit and 32-bit MinGW shells.

**Building**

* If you are on a 64-bit system, run:

        make release-static-win64

* If you are on a 32-bit system, run:

        make release-static-win32

* The resulting executables can be found in `build/release/bin`

### On FreeBSD:

The project can be built from scratch by following instructions for Linux above. If you are running Graft Network in a jail you need to add the flag: `allow.sysvipc=1` to your jail configuration, otherwise lmdb will throw the error message: `Failed to open lmdb environment: Function not implemented`.

We expect to add Graft Network into the ports tree in the near future, which will aid in managing installations using ports or packages.

### On OpenBSD:

This has been tested on OpenBSD 5.8.

You will need to add a few packages to your system. `pkg_add db cmake gcc gcc-libs g++ miniupnpc gtest`.

The doxygen and graphviz packages are optional and require the xbase set.

The Boost package has a bug that will prevent librpc.a from building correctly. In order to fix this, you will have to Build boost yourself from scratch. Follow the directions here (under "Building Boost"):
https://github.com/bitcoin/bitcoin/blob/master/doc/build-openbsd.md

You will have to add the serialization, date_time, and regex modules to Boost when building as they are needed by Graft Network.

To build: `env CC=egcc CXX=eg++ CPP=ecpp DEVELOPER_LOCAL_TOOLS=1 BOOST_ROOT=/path/to/the/boost/you/built make release-static-64`

### Building Portable Statically Linked Binaries

By default, in either dynamically or statically linked builds, binaries target the specific host processor on which the build happens and are not portable to other processors. Portable binaries can be built using the following targets:

* ```make release-static-64``` builds binaries on Linux on x86_64 portable across POSIX systems on x86_64 processors
* ```make release-static-32``` builds binaries on Linux on x86_64 or i686 portable across POSIX systems on i686 processors
* ```make release-static-armv8``` builds binaries on Linux portable across POSIX systems on armv8 processors
* ```make release-static-armv7``` builds binaries on Linux portable across POSIX systems on armv7 processors
* ```make release-static-armv6``` builds binaries on Linux portable across POSIX systems on armv6 processors
* ```make release-static-win64``` builds binaries on 64-bit Windows portable across 64-bit Windows systems
* ```make release-static-win32``` builds binaries on 64-bit or 32-bit Windows portable across 32-bit Windows systems

## Running graftnoded

The build places the binary in `bin/` sub-directory within the build directory
from which cmake was invoked (repository root by default). To run in
foreground:

    ./bin/graftnoded

To list all available options, run `./bin/graftnoded --help`.  Options can be
specified either on the command line or in a configuration file passed by the
`--config-file` argument.  To specify an option in the configuration file, add
a line with the syntax `argumentname=value`, where `argumentname` is the name
of the argument without the leading dashes, for example `log-level=1`.

To run in background:

    ./bin/graftnoded --log-file graftnoded.log --detach

To run as a systemd service, copy
[graftnoded.service](utils/systemd/graftnoded.service) to `/etc/systemd/system/` and
[graftnoded.conf](utils/conf/graftnoded.conf) to `/etc/`. The [example
service](utils/systemd/graftnoded.service) assumes that the user `graftnode` exists
and its home is the data directory specified in the [example
config](utils/conf/graftnoded.conf).

If you're on Mac, you may need to add the `--max-concurrency 1` option to
graft-wallet-cli, and possibly graftnoded, if you get crashes refreshing.

## Internationalization

See [README.i18n.md](README.i18n.md).

## Using Tor

While Graft Network isn't made to integrate with Tor, it can be used wrapped with torsocks, if you add --p2p-bind-ip 127.0.0.1 to the graftnoded command line. You also want to set DNS requests to go over TCP, so they'll be routed through Tor, by setting DNS_PUBLIC=tcp. You may also disable IGD (UPnP port forwarding negotiation), which is pointless with Tor. To allow local connections from the wallet, you might have to add TORSOCKS_ALLOW_INBOUND=1, some OSes need it and some don't. Example:

`DNS_PUBLIC=tcp torsocks graftnoded --p2p-bind-ip 127.0.0.1 --no-igd`

or:

`DNS_PUBLIC=tcp TORSOCKS_ALLOW_INBOUND=1 torsocks graftnoded --p2p-bind-ip 127.0.0.1 --no-igd`

TAILS ships with a very restrictive set of firewall rules. Therefore, you need to add a rule to allow this connection too, in addition to telling torsocks to allow inbound connections. Full example:

`sudo iptables -I OUTPUT 2 -p tcp -d 127.0.0.1 -m tcp --dport 18081 -j ACCEPT`

`DNS_PUBLIC=tcp torsocks ./graftnoded --p2p-bind-ip 127.0.0.1 --no-igd --rpc-bind-ip 127.0.0.1 --data-dir /home/amnesia/Persistent/your/directory/to/the/blockchain`

`./graft-wallet-cli`

## Using readline

While graftnoded and graft-wallet-cli do not use readline directly, most of the functionality can be obtained by running them via rlwrap. This allows command recall, edit capabilities, etc. It does not give autocompletion without an extra completion file, however. To use rlwrap, simply prepend `rlwrap` to the command line, eg:

`rlwrap bin/graft-wallet-cli --wallet-file /path/to/wallet`

Note: rlwrap will save things like your seed and private keys, if you supply them on prompt. You may want to not use rlwrap when you use simplewallet to restore from seed, etc.

# Debugging

This section contains general instructions for debugging failed installs or problems encountered with Graft Network. First ensure you are running the latest version built from the github repo.

## Obtaining Stack Traces and Core Dumps on Unix Systems

We generally use the tool `gdb` (GNU debugger) to provide stack trace functionality, and `ulimit` to provide core dumps in builds which crash or segfault.

* To use gdb in order to obtain a stack trace for a build that has stalled:

Run the build.

Once it stalls, enter the following command:

```
gdb /path/to/graftnoded `pidof graftnoded` 
```

Type `thread apply all bt` within gdb in order to obtain the stack trace

* If however the core dumps or segfaults:

Enter `ulimit -c unlimited` on the command line to enable unlimited filesizes for core dumps

Run the build.

When it terminates with an output along the lines of "Segmentation fault (core dumped)", there should be a core dump file in the same directory as graftnoded.

You can now analyse this core dump with `gdb` as follows:

`gdb /path/to/graftnoded /path/to/dumpfile`

Print the stack trace with `bt`

* To run Graft Network within gdb:

Type `gdb /path/to/graftnoded`

Pass command-line options with `--args` followed by the relevant arguments

Type `run` to run graftnoded

## Analysing Memory Corruption

We use the tool `valgrind` for this.

Run with `valgrind /path/to/graftnoded`. It will be slow.

## LMDB

Instructions for debugging suspected blockchain corruption as per @HYC

There is an `mdb_stat` command in the LMDB source that can print statistics about the database but it's not routinely built. This can be built with the following command:

`cd ~/GraftNetwork/external/db_drivers/liblmdb && make`

The output of `mdb_stat -ea <path to blockchain dir>` will indicate inconsistencies in the blocks, block_heights and block_info table.

The output of `mdb_dump -s blocks <path to blockchain dir>` and `mdb_dump -s block_info <path to blockchain dir>` is useful for indicating whether blocks and block_info contain the same keys.

These records are dumped as hex data, where the first line is the key and the second line is the data.
