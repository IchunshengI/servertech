#
# Copyright (c) 2023-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.16.0)
project(proxy_http_server)

# Boost.Charconv is required by Boost.MySQL
find_package(Boost REQUIRED COMPONENTS system thread json charconv)

# OpenSSL is required
find_package(OpenSSL REQUIRED)

# This library is consumed by the actual server and the tests
add_library(
    proxy_http_server

    # Server
    src/https_proxy.cpp
)

target_link_libraries(
    proxy_http_server
    PUBLIC
    Boost::system
    Boost::thread
    Boost::json
    Boost::charconv
    OpenSSL::SSL
)

target_include_directories(
    proxy_http_server
    PUBLIC
    include
)

# Coroutines require C++20
target_compile_features(
    proxy_http_server
    PUBLIC
    cxx_std_20
)

# The actual server executable
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE proxy_http_server)
 
