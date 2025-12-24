//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/redis_warmup_service.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/core/span.hpp>

#include <array>
#include <chrono>
#include <optional>
#include <string_view>
#include <vector>

#include "error.hpp"
#include "rooms.hpp"
#include "services/mongodb_client.hpp"
#include "services/redis_client.hpp"

using namespace chat;

redis_warmup_service::redis_warmup_service(boost::asio::any_io_executor ex, redis_client& redis, mongodb_client& mongodb)
    : ex_(std::move(ex)), redis_(&redis), mongodb_(&mongodb), stop_(std::make_shared<std::atomic<bool>>(false))
{
}

void redis_warmup_service::start()
{
    auto stop_flag = stop_;
    auto* redis = redis_;
    auto* mongodb = mongodb_;
    auto ex = ex_;

    auto strand = boost::asio::make_strand(ex_);
    boost::asio::spawn(strand, [stop_flag, redis, mongodb, ex](boost::asio::yield_context yield) {
        boost::asio::steady_timer timer(ex);
        boost::system::error_code ec;

        // Retry loop: Redis might be unavailable at startup; Mongo/Redis might transiently fail.
        for (;;)
        {
            if (stop_flag->load())
                return;

            bool all_rooms_warm = true;

            // Check each known room.
            for (auto room_id : room_ids)
            {
                if (stop_flag->load())
                    return;

                // Ask Redis for history; if it has any messages, consider the room warm.
                std::array<redis_client::room_histoy_request, 1> reqs{
                    redis_client::room_histoy_request{room_id, std::optional<std::string_view>{}}
                };
                auto hist = redis->get_room_history(reqs, yield);
                if (hist.has_error())
                {
                    all_rooms_warm = false;
                    log_error(hist.error(), "redis warmup: redis history failed");
                    break;
                }

                if (!hist->empty() && !hist->front().messages.empty())
                    continue;

                all_rooms_warm = false;

                // Redis is empty: fetch latest from Mongo and backfill Redis.
                auto mongo_messages = mongodb->get_latest_room_messages(room_id, redis_client::message_batch_size);
                if (mongo_messages.has_error())
                {
                    log_error(mongo_messages.error(), "redis warmup: mongodb query failed");
                    continue;
                }

                if (mongo_messages->empty())
                    continue;

                // Mongo returns newest first; Redis insert expects oldest first.
                std::vector<message> chronological(mongo_messages->rbegin(), mongo_messages->rend());
                boost::span<const message> span(chronological.data(), chronological.size());
                auto ids = redis->store_messages(room_id, span, yield);
                if (ids.has_error())
                {
                    log_error(ids.error(), "redis warmup: redis store failed");
                    continue;
                }
            }

            if (all_rooms_warm)
                return;

            timer.expires_after(std::chrono::seconds(2));
            timer.async_wait(yield[ec]);
            ec.clear();
        }
    });
}

void redis_warmup_service::stop()
{
    if (stop_)
        stop_->store(true);
}
