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

set(supernode_tests_sources
  main.cpp

)

set(supernode_tests_headers
  )

add_executable(supernode_tests
  ${supernode_tests_sources}
  ${supernode_tests_headers})

target_link_libraries(supernode_tests
  PRIVATE
    wallet
    epee
    supernode
    ${Boost_CHRONO_LIBRARY}
    ${Boost_SERIALIZATION_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${GTEST_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})

set_property(TARGET supernode_tests
  PROPERTY
    FOLDER "tests")

if (NOT MSVC)
  set_property(TARGET supernode_tests
    APPEND_STRING
    PROPERTY
      COMPILE_FLAGS " -Wno-undef -Wno-sign-compare")
endif ()

add_test(
  NAME    supernode_tests
  COMMAND supernode_tests)
