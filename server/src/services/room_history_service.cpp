//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/room_history_service.hpp"

#include <array>
#include <string_view>
#include <unordered_set>
#include <iostream>
#include "error.hpp"
#include "business_types.hpp"
#include "services/mongodb_client.hpp"
#include "services/mysql_client.hpp"
#include "services/redis_client.hpp"

using namespace chat;

static std::vector<std::int64_t> unique_user_ids(const std::vector<message_batch>& input)
{
    std::unordered_set<std::int64_t> set;
    for (const auto& batch : input)
        for (const auto& msg : batch.messages)
            set.insert(msg.user_id);
    return std::vector<std::int64_t>(set.begin(), set.end());
}

result_with_message<std::pair<std::vector<message_batch>, username_map>> room_history_service::
    get_room_history(boost::span<const std::string_view> room_ids, boost::asio::yield_context yield)
{
    // Compose an array of requests for Redis
    std::vector<redis_client::room_histoy_request> redis_req;
    redis_req.resize(room_ids.size());
    for (std::size_t i = 0; i < room_ids.size(); ++i)
        redis_req[i].room_id = room_ids[i];

    // Lookup messages
    auto batches_result = redis_->get_room_history(redis_req, yield);
    if (batches_result.has_error())
        return std::move(batches_result).error();
    assert(batches_result->size() == room_ids.size());

    // Lazy fallback: if Redis cache is empty for a room, load from MongoDB and backfill Redis.
    for (std::size_t i = 0; i < room_ids.size(); ++i)
    {
        auto& batch = (*batches_result)[i];
        if (!batch.messages.empty())
            continue;

        // Fetch N+1 to compute has_more
        const auto mongo_messages = mongodb_->get_latest_room_messages(
            room_ids[i],
            redis_client::message_batch_size + 1
        );
        if (mongo_messages.has_error())
            return std::move(mongo_messages).error();

        auto msgs = std::move(*mongo_messages);  // newest first
        batch.has_more = msgs.size() > redis_client::message_batch_size;
        if (msgs.size() > redis_client::message_batch_size)
            msgs.resize(redis_client::message_batch_size);
        batch.messages = msgs;  // newest first, matching Redis XREVRANGE order

        // Backfill Redis stream in chronological order so the stream order matches message time.
        if (!batch.messages.empty())
        {
            std::vector<message> chronological(batch.messages.rbegin(), batch.messages.rend());  // oldest first
            auto err = redis_->store_messages_with_ids(room_ids[i], chronological, yield);
            if (err.ec)
                log_error(err, "Redis backfill failed");
        }
    }

    // Collect the IDs we need to lookup
    auto user_ids = unique_user_ids(*batches_result);

    // Look them up
    auto usernames_result = mysql_->get_usernames(user_ids, yield);
    if (usernames_result.has_error())
        return std::move(usernames_result).error();

    return std::pair{std::move(*batches_result), std::move(*usernames_result)};
}

result_with_message<std::pair<message_batch, username_map>> room_history_service::get_room_history(
    std::string_view room_id,
    boost::asio::yield_context yield
)
{
    std::cout << __FUNCTION__ << "  room_id  " << room_id << std::endl;
    // Compose an aray with a single request
    std::array<std::string_view, 1> room_ids{room_id};

    // Call the batch function
    auto res = get_room_history(room_ids, yield);
    if (res.has_error())
        return std::move(res).error();
    assert(res->first.size() == 1u);

    // Result
    return std::pair{std::move(res->first.front()), std::move(res->second)};
}
