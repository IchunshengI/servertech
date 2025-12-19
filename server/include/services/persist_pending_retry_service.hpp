//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_PERSIST_PENDING_RETRY_SERVICE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_PERSIST_PENDING_RETRY_SERVICE_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>

#include <atomic>
#include <chrono>
#include <memory>

namespace chat {

class redis_client;

// Periodically retries persistence tasks stored in Redis "persist_pending" stream
// by calling the sync service /publish endpoint.
class persist_pending_retry_service
{
    boost::asio::any_io_executor ex_;
    redis_client* redis_{};
    std::shared_ptr<std::atomic<bool>> stop_;

public:
    persist_pending_retry_service(boost::asio::any_io_executor ex, redis_client& redis);

    void start();
    void stop();
};

}  // namespace chat

#endif

