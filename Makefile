# Copyright (c) 2018, The Loki Project
# Copyright (c) 2014-2019, The Monero Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ANDROID_STANDALONE_TOOLCHAIN_PATH ?= /usr/local/toolchain

dotgit=$(shell ls -d .git/config)
ifneq ($(dotgit), .git/config)
  USE_SINGLE_BUILDDIR=1
endif

subbuilddir:=$(shell echo  `uname | sed -e 's|[:/\\ \(\)]|_|g'`/`git branch | grep '\* ' | cut -f2- -d' '| sed -e 's|[:/\\ \(\)]|_|g'`)
ifeq ($(USE_SINGLE_BUILDDIR),)
  builddir := build/"$(subbuilddir)"
  topdir   := ../../../..
  deldirs  := $(builddir)
else
  builddir := build
  topdir   := ../..
  deldirs  := $(builddir)/debug $(builddir)/release $(builddir)/fuzz
endif

all: release-all

depends:
	cd contrib/depends && $(MAKE) HOST=$(target) && cd ../.. && mkdir -p build/$(target)/release
	cd build/$(target)/release && cmake -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/contrib/depends/$(target)/share/toolchain.cmake ../../.. && $(MAKE)

cmake-debug:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D CMAKE_BUILD_TYPE=Debug $(topdir)

debug: cmake-debug
	cd $(builddir)/debug && $(MAKE)

# Temporarily disable some tests:
#  * libwallet_api_tests fail (Issue #895)
debug-test:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D BUILD_TESTS=ON -D CMAKE_BUILD_TYPE=Debug $(topdir) &&  $(MAKE) && $(MAKE) ARGS="-E libwallet_api_tests" test

integration:
	mkdir -p $(builddir)/integration
	cd $(builddir)/integration && cmake -D CMAKE_BUILD_TYPE=Debug -D BUILD_INTEGRATION=ON $(topdir) &&  $(MAKE)

debug-test-trezor:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D BUILD_TESTS=ON -D TREZOR_DEBUG=ON -D CMAKE_BUILD_TYPE=Debug $(topdir) &&  $(MAKE) && $(MAKE) ARGS="-E libwallet_api_tests" test

debug-all:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D BUILD_TESTS=ON -D BUILD_SHARED_LIBS=OFF -D CMAKE_BUILD_TYPE=Debug $(topdir) && $(MAKE)

debug-static-all:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D BUILD_TESTS=ON -D STATIC=ON -D CMAKE_BUILD_TYPE=Debug $(topdir) && $(MAKE)

debug-static-test:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D BUILD_TESTS=ON -D STATIC=ON -D CMAKE_BUILD_TYPE=Debug $(topdir) && $(MAKE) && $(MAKE) ARGS="-E libwallet_api_tests" test

debug-static-win64:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -G "MSYS Makefiles" -D STATIC=ON -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Debug -D BUILD_TAG="win-x64" -D CMAKE_TOOLCHAIN_FILE=$(topdir)/cmake/64-bit-toolchain.cmake -D MSYS2_FOLDER=c:/msys64 $(topdir) && $(MAKE)
 
debug-static-win32:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -G "MSYS Makefiles" -D STATIC=ON -D ARCH="i686" -D BUILD_64=OFF -D CMAKE_BUILD_TYPE=Debug -D BUILD_TAG="win-x32" -D CMAKE_TOOLCHAIN_FILE=$(topdir)/cmake/32-bit-toolchain.cmake -D MSYS2_FOLDER=c:/msys32 $(topdir) && $(MAKE)
 
cmake-release:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D CMAKE_BUILD_TYPE=Release -D WARNINGS_AS_ERRORS=ON $(topdir)

cmake-release-syslog:
	mkdir -p build/release
	cd build/release && cmake -D CMAKE_BUILD_TYPE=Release -D ENABLE_SYSLOG=ON ../..

cmake-release-syslog-static:
	mkdir -p build/release
	cd build/release && cmake -D CMAKE_BUILD_TYPE=Release -D ENABLE_SYSLOG=ON -D STATIC=ON ../..

release: cmake-release
	cd $(builddir)/release && $(MAKE)

release-syslog: cmake-release-syslog
	cd build/release && $(MAKE)

release-syslog-static: cmake-release-syslog-static
	cd build/release && $(MAKE)

release-test:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D BUILD_TESTS=ON -D CMAKE_BUILD_TYPE=release -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE) && $(MAKE) test

release-all:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D BUILD_TESTS=ON -D CMAKE_BUILD_TYPE=release -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D STATIC=ON -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=release -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-locking:
	mkdir -p build/release
	cd build/release && cmake -D STATIC=ON -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=release ../.. && $(MAKE) CXX_FLAGS=-DLOCK_RTA_SENDING

coverage:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D BUILD_TESTS=ON -D CMAKE_BUILD_TYPE=Debug -D COVERAGE=ON $(topdir) && $(MAKE) && $(MAKE) test

# Targets for specific prebuilt builds which will be advertised for updates by their build tag

release-static-linux-armv6:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D BUILD_TESTS=OFF -D ARCH="armv6zk" -D STATIC=ON -D BUILD_64=OFF -D CMAKE_BUILD_TYPE=release -D BUILD_TAG="linux-armv6" -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-linux-armv7:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D BUILD_TESTS=OFF -D ARCH="armv7-a" -D STATIC=ON -D BUILD_64=OFF -D CMAKE_BUILD_TYPE=release -D BUILD_TAG="linux-armv7" -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-android-armv7:
	mkdir -p $(builddir)/release/translations
	cd $(builddir)/release/translations && cmake ../../../translations && $(MAKE)
	cd $(builddir)/release && CC=arm-linux-androideabi-clang CXX=arm-linux-androideabi-clang++ cmake -D BUILD_TESTS=OFF -D ARCH="armv7-a" -D STATIC=ON -D BUILD_64=OFF -D CMAKE_BUILD_TYPE=release -D ANDROID=true -D INSTALL_VENDORED_LIBUNBOUND=ON -D BUILD_TAG="android-armv7" -D CMAKE_SYSTEM_NAME="Android" -D CMAKE_ANDROID_STANDALONE_TOOLCHAIN="${ANDROID_STANDALONE_TOOLCHAIN_PATH}" -D CMAKE_ANDROID_ARM_MODE=ON -D CMAKE_ANDROID_ARCH_ABI="armeabi-v7a" -D WARNINGS_AS_ERRORS=ON ../.. && $(MAKE)

release-static-android-armv8:
	mkdir -p $(builddir)/release/translations
	cd $(builddir)/release/translations && cmake ../../../translations && $(MAKE)
	cd $(builddir)/release && CC=aarch64-linux-android-clang CXX=aarch64-linux-android-clang++ cmake -D BUILD_TESTS=OFF -D ARCH="armv8-a" -D STATIC=ON -D BUILD_64=ON -D CMAKE_BUILD_TYPE=release -D ANDROID=true -D INSTALL_VENDORED_LIBUNBOUND=ON -D BUILD_TAG="android-armv8" -D CMAKE_SYSTEM_NAME="Android" -D CMAKE_ANDROID_STANDALONE_TOOLCHAIN="${ANDROID_STANDALONE_TOOLCHAIN_PATH}" -D CMAKE_ANDROID_ARCH_ABI="arm64-v8a" -D WARNINGS_AS_ERRORS=ON ../.. && $(MAKE)

release-static-linux-armv8:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D BUILD_TESTS=OFF -D ARCH="armv8-a" -D STATIC=ON -D BUILD_64=ON -D CMAKE_BUILD_TYPE=release -D BUILD_TAG="linux-armv8" -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-linux-x86_64:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D STATIC=ON -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=release -D BUILD_TAG="linux-x64" -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-linux-x86_64-debug-info:
	mkdir -p build/release
	cd build/release && cmake -D STATIC=ON -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=RelWithDebInfo -D BUILD_TAG="linux-x64" ../.. && $(MAKE)

release-static-freebsd-x86_64:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D STATIC=ON -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=release -D BUILD_TAG="freebsd-x64" -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-mac-x86_64:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D STATIC=ON -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=release -D BUILD_TAG="mac-x64" -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-linux-i686:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -D STATIC=ON -D ARCH="i686" -D BUILD_64=OFF -D CMAKE_BUILD_TYPE=release -D BUILD_TAG="linux-x86" -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-win64:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -G "MSYS Makefiles" -D STATIC=ON -D ARCH="x86-64" -D BUILD_64=ON -D CMAKE_BUILD_TYPE=Release -D BUILD_TAG="win-x64" -D CMAKE_TOOLCHAIN_FILE=$(topdir)/cmake/64-bit-toolchain.cmake -D MSYS2_FOLDER=c:/msys64 -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

release-static-win32:
	mkdir -p $(builddir)/release
	cd $(builddir)/release && cmake -G "MSYS Makefiles" -D STATIC=ON -D ARCH="i686" -D BUILD_64=OFF -D CMAKE_BUILD_TYPE=Release -D BUILD_TAG="win-x32" -D CMAKE_TOOLCHAIN_FILE=$(topdir)/cmake/32-bit-toolchain.cmake -D MSYS2_FOLDER=c:/msys32 -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE)

fuzz:
	mkdir -p $(builddir)/fuzz
	cd $(builddir)/fuzz && cmake -D STATIC=ON -D SANITIZE=ON -D BUILD_TESTS=ON -D USE_LTO=OFF -D CMAKE_C_COMPILER=afl-gcc -D CMAKE_CXX_COMPILER=afl-g++ -D ARCH="x86-64" -D CMAKE_BUILD_TYPE=fuzz -D BUILD_TAG="linux-x64" $(topdir) && $(MAKE)

clean:
	@echo "WARNING: Back-up your wallet if it exists within ./"$(deldirs)"!" ; \
    read -r -p "This will destroy the build directory, continue (y/N)?: " CONTINUE; \
	[ $$CONTINUE = "y" ] || [ $$CONTINUE = "Y" ] || (echo "Exiting."; exit 1;)
	rm -rf $(deldirs)

clean-all:
	@echo "WARNING: Back-up your wallet if it exists within ./build!" ; \
	read -r -p "This will destroy all build directories, continue (y/N)?: " CONTINUE; \
	[ $$CONTINUE = "y" ] || [ $$CONTINUE = "Y" ] || (echo "Exiting."; exit 1;)
	rm -rf ./build

tags:
	ctags -R --sort=1 --c++-kinds=+p --fields=+iaS --extra=+q --language-force=C++ src contrib tests/gtest tests

# Debug Target for Developers: Only build daemon and wallet
developer-daemon-and-wallet:
	mkdir -p $(builddir)/debug
	cd $(builddir)/debug && cmake -D CMAKE_BUILD_TYPE=Debug -D BUILD_TESTS=OFF -D WARNINGS_AS_ERRORS=ON $(topdir) && $(MAKE) daemon simplewallet

.PHONY: all cmake-debug debug debug-test debug-all cmake-release release release-test release-all clean tags developer_daemon_and_wallet
