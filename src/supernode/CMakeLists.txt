# Copyright (c) 2017, The Graft Project
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
#
# Parts of this file are originally copyright (c) 2014-2017 The Monero Project

# include (${PROJECT_SOURCE_DIR}/cmake/libutils.cmake)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(supernode_sources
    api/pending_transaction.cpp
    graft_wallet.cpp
    wallet_args.cpp
    node_rpc_proxy.cpp
    AuthSample.cpp
    AuthSampleObject.cpp
    BaseRTAObject.cpp
    BaseRTAProcessor.cpp
    baseclientproxy.cpp
    DAPI_RPC_Client.cpp
    DAPI_RPC_Server.cpp
    FSN_Servant.cpp
    PosProxy.cpp
    PosSaleObject.cpp
    SubNetBroadcast.cpp
    WalletPayObject.cpp
    WalletProxy.cpp
    P2P_Broadcast.cpp
    supernode_common_struct.cpp
    supernode_rpc_command.cpp
    FSN_ServantBase.cpp)

set(supernode_api_headers)

set(supernode_private_headers
    api/pending_transaction.h
    graft_defines.h
    graft_wallet.h
    wallet_args.h
    wallet_errors.h
    node_rpc_proxy.h
    AuthSample.h
    AuthSampleObject.h
    BaseRTAObject.h
    BaseRTAProcessor.h
    baseclientproxy.h
    DAPI_RPC_Client.h
    DAPI_RPC_Server.h
    FSN_Servant.h
    PosProxy.h
    PosSaleObject.h
    SubNetBroadcast.h
    WalletPayObject.h
    WalletProxy.h
    P2P_Broadcast.h
    supernode_common_struct.h
    supernode_rpc_command.h)

monero_private_headers(supernode
    ${supernode_private_headers})
monero_add_library(supernode
    ${supernode_sources}
    ${supernode_api_headers}
    ${supernode_private_headers})

# static libs cant be linked with another libs
#target_link_libraries(supernode
#    PUBLIC
#    rpc
#    daemon_lib
#    common
#    cryptonote_core
#    mnemonics
#    p2p
#    uuid
#    blockchain_db
#    cncrypto
#    cryptonote_protocol
#    daemonizer
#    ${Boost_CHRONO_LIBRARY}
#    ${Boost_SERIALIZATION_LIBRARY}
#    ${Boost_FILESYSTEM_LIBRARY}
#    ${Boost_SYSTEM_LIBRARY}
#    ${Boost_THREAD_LIBRARY}
#    ${Boost_REGEX_LIBRARY}
#    PRIVATE
#    ${EXTRA_LIBRARIES})
#add_dependencies(supernode version)

if (NOT BUILD_GUI_DEPS)
    set(supernode_rpc_sources
        main.cpp
        AuthSample.cpp
        AuthSampleObject.cpp
        BaseRTAObject.cpp
        BaseRTAProcessor.cpp
        DAPI_RPC_Client.cpp
        DAPI_RPC_Server.cpp
        FSN_Servant.cpp
        PosProxy.cpp
        PosSaleObject.cpp
        SubNetBroadcast.cpp
        WalletPayObject.cpp
        WalletProxy.cpp
        P2P_Broadcast.cpp
        supernode_common_struct.cpp
        supernode_rpc_command.cpp
        FSN_ServantBase.cpp)

    set(supernode_rpc_headers)

    set(supernode_rpc_private_headers)

    monero_private_headers(supernode_rpc_server
        ${supernode_rpc_private_headers})
    monero_add_executable(supernode_rpc_server
        ${supernode_rpc_sources}
        ${supernode_rpc_headers}
        ${supernode_rpc_private_headers})

    target_link_libraries(supernode_rpc_server
        PRIVATE
        supernode
        daemon_lib
        wallet
        epee
        rpc
        cryptonote_core
        cryptonote_protocol
        cncrypto
        common
        uuid
        daemonizer
        mnemonics
        p2p
        uuid
        blockchain_db
        ${Boost_CHRONO_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
        ${EXTRA_LIBRARIES})
    add_dependencies(supernode_rpc_server version)
    set_property(TARGET supernode_rpc_server
        PROPERTY
        OUTPUT_NAME "graft-supernode")
    install(TARGETS supernode_rpc_server DESTINATION bin)
endif()


# build and install libsupernode_merged only if we building for GUI
if (BUILD_GUI_DEPS)
    set(libs_to_merge supernode cryptonote_core cryptonote_basic mnemonics common cncrypto ringct)

    foreach(lib ${libs_to_merge})
        list(APPEND objlibs $<TARGET_OBJECTS:obj_${lib}>) # matches naming convention in src/CMakeLists.txt
    endforeach()
    add_library(supernode_merged STATIC ${objlibs})
    if(IOS)
        set(lib_folder lib-${ARCH})
    else()
        set(lib_folder lib)
    endif()
    install(TARGETS supernode_merged
        ARCHIVE DESTINATION ${lib_folder})

    install(FILES ${supernode_api_headers}
        DESTINATION include/supernode)
endif()
