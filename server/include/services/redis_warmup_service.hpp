//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_WARMUP_SERVICE_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_WARMUP_SERVICE_HPP

#include <boost/asio/any_io_executor.hpp>

#include <atomic>
#include <memory>

namespace chat {

class redis_client;
class mongodb_client;

// On server startup, if Redis is empty for a room, backfill it from MongoDB.
class redis_warmup_service
{
    boost::asio::any_io_executor ex_;
    redis_client* redis_{};
    mongodb_client* mongodb_{};
    std::shared_ptr<std::atomic<bool>> stop_;

public:
    redis_warmup_service(boost::asio::any_io_executor ex, redis_client& redis, mongodb_client& mongodb);

    void start();
    void stop();
};

}  // namespace chat

#endif

