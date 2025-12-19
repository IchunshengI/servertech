//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/redis_client.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/redis/adapter/result.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "error.hpp"
#include "services/redis_serialization.hpp"

using namespace chat;

namespace {

static constexpr std::string_view k_persist_pending_stream = "persist_pending";

class redis_client_impl final : public redis_client
{
    boost::redis::connection conn_;

public:
    redis_client_impl(boost::asio::any_io_executor ex) : conn_(std::move(ex)) {}

    /* 创建对象 */
    void start_run() final override
    {
        // The host to connect to. Defaults to localhost
        // const char* host_c_str = std::getenv("REDIS_HOST");
        // std::string host = host_c_str ? host_c_str : "localhost";
        std::string host = "redis-servertech";

        boost::redis::config cfg;
        cfg.addr.host = std::move(host);
        cfg.health_check_interval = std::chrono::seconds::zero();  // Disable health checks for now
        conn_.async_run(cfg, {}, boost::asio::detached);  // 异步连接redis
    }

    void cancel() final override { conn_.cancel(); }

    result_with_message<std::vector<message_batch>> get_room_history(
        boost::span<const room_histoy_request> input,
        boost::asio::yield_context yield
    ) final override
    {
        assert(!input.empty());

        // Compose the request. XREVRANGE will get all messages for a room,
        // since the beginning, in reverse order, up to message_batch_size
        boost::redis::request req;
        for (const auto& room_req : input)
        {
            std::string stream_ref = room_req.last_message_id ? "(" : "+";
            if (room_req.last_message_id)
                stream_ref.append(*room_req.last_message_id);
            req.push("XREVRANGE", room_req.room_id, stream_ref, "-", "COUNT", message_batch_size);
        }

        // Run it
        boost::redis::generic_response res;
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);
        if (ec)
            return error_with_message{ec};

        // Verify success. If any of the nodes contains a Redis error (e.g.
        // because we sent an invalid command), this will contain an error.
        if (res.has_error())
            CHAT_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(res).error().diagnostic)

        // Parse the response
        auto result = parse_room_history_batch(*res);
        if (result.has_error())
            return error_with_message{result.error()};

        // Set the has_more flag
        for (auto& batch : *result)
            batch.has_more = batch.messages.size() >= message_batch_size;

        return std::move(*result);
    }

    result_with_message<std::vector<std::string>> store_messages(
        std::string_view room_id,
        boost::span<const message> messages,
        boost::asio::yield_context yield
    ) final override
    {
        // Compose the request. This appends a message to the given room and
        // auto-assigns it an ID.
        boost::redis::request req;
        for (const auto& msg : messages)
            req.push("XADD", room_id, "*", "payload", serialize_redis_message(msg)); /* 流不存在时redis会自动创建 */

        // Execute it
        boost::redis::generic_response res;
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);
        if (ec)
            return error_with_message{ec};

        // Verify success. If any of the nodes contains a Redis error (e.g.
        // because we sent an invalid command), this will contain an error.
        if (res.has_error())
            CHAT_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(res).error().diagnostic)

        // Parse the response
        auto result = parse_batch_xadd_response(*res);
        if (result.has_error())
            return error_with_message{result.error()};
        return std::move(*result);
    }

    result_with_message<std::pair<std::vector<std::string>, std::vector<std::string>>> store_messages_with_pending(
        std::string_view room_id,
        boost::span<const message> messages,
        boost::asio::yield_context yield
    ) final override
    {
        // Lua script: for each payload, XADD to room stream, then XADD to persist_pending with room_id+redis_id+payload.
        static constexpr std::string_view script = R"lua(
local room = KEYS[1]
local pending = KEYS[2]
local res = {}
for i = 1, #ARGV do
  local payload = ARGV[i]
  local id = redis.call('XADD', room, '*', 'payload', payload)
  local pid = redis.call('XADD', pending, '*', 'room_id', room, 'redis_id', id, 'payload', payload)
  table.insert(res, id)
  table.insert(res, pid)
end
return res
)lua";

        boost::redis::request req;
        req.push(
            "EVAL",
            script,
            "2",
            room_id,
            k_persist_pending_stream
        );
        for (const auto& msg : messages)
            req.push(serialize_redis_message(msg));

        boost::redis::generic_response res;
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);
        if (ec)
            return error_with_message{ec};

        if (res.has_error())
            CHAT_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(res).error().diagnostic)

        auto flat = parse_string_array_response(*res);
        if (flat.has_error())
            return error_with_message{flat.error()};

        if (flat->size() != messages.size() * 2)
            return error_with_message{errc::redis_parse_error, "Unexpected EVAL response size"};

        std::vector<std::string> ids;
        std::vector<std::string> pending_ids;
        ids.reserve(messages.size());
        pending_ids.reserve(messages.size());
        for (std::size_t i = 0; i < flat->size(); i += 2)
        {
            ids.push_back(std::move((*flat)[i]));
            pending_ids.push_back(std::move((*flat)[i + 1]));
        }

        return std::pair{std::move(ids), std::move(pending_ids)};
    }

    result_with_message<std::vector<persist_pending_task>> get_persist_pending_tasks(
        std::size_t limit,
        boost::asio::yield_context yield
    ) final override
    {
        boost::redis::request req;
        req.push("XRANGE", k_persist_pending_stream, "-", "+", "COUNT", limit);

        boost::redis::generic_response res;
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);
        if (ec)
            return error_with_message{ec};

        if (res.has_error())
            CHAT_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(res).error().diagnostic)

        auto parsed = parse_persist_pending_xrange_response(*res);
        if (parsed.has_error())
            return error_with_message{parsed.error()};

        std::vector<persist_pending_task> out;
        out.reserve(parsed->size());
        for (auto& e : *parsed)
        {
            out.push_back(persist_pending_task{
                std::move(e.entry_id),
                std::move(e.room_id),
                std::move(e.redis_id),
                std::move(e.payload),
            });
        }

        return out;
    }

    error_with_message delete_stream_entries(
        std::string_view stream_key,
        boost::span<const std::string> entry_ids,
        boost::asio::yield_context yield
    ) final override
    {
        if (entry_ids.empty())
            return {};

        boost::redis::request req;
        req.push("XDEL", stream_key);
        for (const auto& id : entry_ids)
            req.push(id);

        boost::redis::response<std::int64_t> res;
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);
        if (ec)
            return error_with_message{ec};

        auto& result = std::get<0>(res);
        if (result.has_error())
            CHAT_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(result).error().diagnostic)

        return {};
    }

    error_with_message store_messages_with_ids(
        std::string_view room_id,
        boost::span<const message> messages,
        boost::asio::yield_context yield
    ) final override
    {
        boost::redis::request req;
        for (const auto& msg : messages)
            req.push("XADD", room_id, msg.id, "payload", serialize_redis_message(msg));

        boost::redis::generic_response res;
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);
        if (ec)
            return error_with_message{ec};

        if (res.has_error())
            CHAT_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(res).error().diagnostic)

        // Validate shape
        auto ids = parse_batch_xadd_response(*res);
        if (ids.has_error())
            return error_with_message{ids.error()};

        return {};
    }

    error_with_message set_nonexisting_key(
        std::string_view key,
        std::string_view value,
        std::chrono::seconds ttl,
        boost::asio::yield_context yield
    ) final override
    {
        // Compose the request. NX prevents key overwrites, EX sets the TTL
        boost::redis::request req;
        req.push("SET", key, value, "NX", "EX", ttl.count());

        // Execute it
        boost::redis::response<std::optional<std::string>> res;
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);
        if (ec)
            return error_with_message{ec};

        // Check
        auto& result = std::get<0>(res);
        if (result.has_error())
            return error_with_message{errc::redis_parse_error, std::move(result).error().diagnostic};
        return result.value().has_value() ? error_with_message{} : error_with_message{errc::already_exists};
    }

    result_with_message<std::int64_t> get_int_key(std::string_view key, boost::asio::yield_context yield)
        final override
    {
        // Compose the request
        boost::redis::request req;
        req.push("GET", key);

        // Execute it
        boost::redis::response<std::optional<std::int64_t>> res;
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);
        if (ec)
            return error_with_message{ec};

        // Check for errors
        auto& result = std::get<0>(res);
        if (result.has_error())
            return error_with_message{errc::redis_parse_error, std::move(result).error().diagnostic};
        auto opt = result.value();

        // Check whether the key was present
        if (opt.has_value())
            return opt.value();
        else{
           // std::cout << "not found get_int_key" <<std::endl;
            return error_with_message{errc::not_found};
        }
    }

    error_with_message delete_key(
        std::string_view key,
        boost::asio::yield_context yield
    ) final override
    {
        /* 构造redis DEL命令 */
        boost::redis::request req;
        req.push("DEL", key);

        /* 执行命令 */
        boost::redis::response<int64_t> res;    /* DEL命令返回的删除的键数量 */
        error_code ec;
        conn_.async_exec(req, res, yield[ec]);

        if(ec){
            return error_with_message{ec};
        }

        auto& result = std::get<0>(res);
        if(result.has_error())
            CHAT_RETURN_ERROR_WITH_MESSAGE(errc::redis_command_failed, std::move(result).error().diagnostic);

        if(result.value() == 0)
            return error_with_message{errc::not_found};
        else
            return error_with_message{};
    }
};

}  // namespace

std::unique_ptr<redis_client> chat::create_redis_client(boost::asio::any_io_executor ex)
{
    return std::unique_ptr<redis_client>{new redis_client_impl(std::move(ex))};
}
