//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/persist_pending_retry_service.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>

#include <array>
#include <chrono>
#include <cstdlib>
#include <string>

#include "error.hpp"
#include "services/persist_http_publisher.hpp"
#include "services/redis_client.hpp"

using namespace chat;

namespace {

static std::int64_t getenv_i64(const char* key, std::int64_t default_value)
{
    const char* v = std::getenv(key);
    if (!v || !*v)
        return default_value;
    try
    {
        return std::stoll(v);
    }
    catch (...)
    {
        return default_value;
    }
}

static boost::json::object build_publish_payload(
    const redis_client::persist_pending_task& task
)
{
    boost::json::object out;
    out["room_id"] = task.room_id;
    out["redis_id"] = task.redis_id;

    // task.payload is the "redis message" JSON stored in the room stream (content/timestamp/user_id).
    boost::system::error_code ec;
    auto parsed = boost::json::parse(task.payload, ec);
    if (!ec)
    {
        if (auto* obj = parsed.if_object())
        {
            if (auto it = obj->find("content"); it != obj->end())
                out["content"] = it->value();
            if (auto it = obj->find("timestamp"); it != obj->end())
                out["timestamp"] = it->value();
            if (auto it = obj->find("user_id"); it != obj->end())
                out["user_id"] = it->value();
        }
    }

    return out;
}

}  // namespace

persist_pending_retry_service::persist_pending_retry_service(boost::asio::any_io_executor ex, redis_client& redis)
    : ex_(std::move(ex)), redis_(&redis), stop_(std::make_shared<std::atomic<bool>>(false))
{
}

void persist_pending_retry_service::start()
{
    auto stop_flag = stop_;
    auto* redis = redis_;
    auto ex = ex_;

    const auto interval_ms = getenv_i64("PERSIST_RETRY_INTERVAL_MS", 2000);
    const auto batch_size = static_cast<std::size_t>(getenv_i64("PERSIST_RETRY_BATCH", 50));

    auto strand = boost::asio::make_strand(ex_);
    boost::asio::spawn(strand, [stop_flag, redis, ex, interval_ms, batch_size](boost::asio::yield_context yield) {
        boost::asio::steady_timer timer(ex);
        boost::system::error_code ec;

        while (!stop_flag->load())
        {
            // Drain a small batch of pending tasks and try publishing them.
            auto tasks = redis->get_persist_pending_tasks(batch_size, yield);
            if (!tasks.has_error())
            {
                for (const auto& task : *tasks)
                {
                    if (stop_flag->load())
                        break;

                    auto payload = build_publish_payload(task);
                    auto err = publish_persist_http(ex, payload, yield);
                    if (!err.ec)
                    {
                        std::array<std::string, 1> ids{task.entry_id};
                        auto del_err = redis->delete_stream_entries("persist_pending", ids, yield);
                        if (del_err.ec)
                            log_error(del_err, "persist pending delete failed");
                    }
                }
            }

            timer.expires_after(std::chrono::milliseconds(interval_ms));
            timer.async_wait(yield[ec]);
            ec.clear();
        }
    });
}

void persist_pending_retry_service::stop()
{
    if (stop_)
        stop_->store(true);
}
