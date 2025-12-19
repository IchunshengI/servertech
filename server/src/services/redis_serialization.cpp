//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/redis_serialization.hpp"

#include <boost/describe/class.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>
#include <boost/redis/resp3/type.hpp>

#include <optional>
#include <string>

#include "error.hpp"
#include "timestamp.hpp"

namespace resp3 = boost::redis::resp3;
using namespace chat;

namespace {

// Message as stored in Redis.
// New messages include the MongoDB _id as "id". Old messages (before MongoDB became
// the source of truth) may not include it, in which case we fall back to the Redis
// stream ID.
struct redis_wire_message
{
    std::optional<std::string_view> id;
    std::string_view content;
    std::int64_t timestamp;
    std::int64_t user_id;
};
BOOST_DESCRIBE_STRUCT(redis_wire_message, (), (id, content, timestamp, user_id))

}  // namespace

static message to_message(const redis_wire_message& from, std::string id)
{
    if (from.id && !from.id->empty())
        id = std::string(*from.id);
    return chat::message{
        std::move(id),
        std::string(from.content),
        parse_timestamp(from.timestamp),
        from.user_id,
    };
}

result<std::vector<message_batch>> chat::parse_room_history_batch(node_span nodes)
{
    std::vector<message_batch> res;
    error_code ec;

    // We need a one-pass parser. Every response has the following format:
    // list of MessageEntry:
    //    MessageEntry[0]: string (id)
    //    MessageEntry[1]: list<string> (key-value pairs; always an even number)
    // Since manipulating nodes is cumbersome, we have a single key named "payload",
    // with a single value containing a JSON
    // This function is capable of parsing multiple, batched responses
    enum state_t
    {
        wants_level0_list,
        wants_level0_or_entry_list,
        wants_id,
        wants_attr_list,
        wants_key,
        wants_value
    };

    struct parser_data_t
    {
        state_t state{wants_level0_list};
        const std::string* id{};
    } data;

    for (const auto& node : nodes)
    {
        if (data.state == wants_level0_list)
        {
            // The top-level list, indicating a new response
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 0u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            res.emplace_back();
            data.state = wants_level0_or_entry_list;
        }
        else if (data.state == wants_level0_or_entry_list)
        {
            // We need either a new response or a new message in the current response
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth == 0u)
            {
                // New response
                res.emplace_back();
                data.state = wants_level0_or_entry_list;
            }
            else if (node.depth == 1u)
            {
                // New message
                if (node.aggregate_size != 2u)
                    CHAT_RETURN_ERROR(errc::redis_parse_error)
                data.state = wants_id;
            }
            else
            {
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            }
        }
        else if (data.state == wants_id)
        {
            // We're waiting for the stream ID field
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.id = &node.value;
            data.state = wants_attr_list;
        }
        else if (data.state == wants_attr_list)
        {
            // We're waiting for the stream record attribute list
            if (node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.aggregate_size != 2u)  // single key/value pair, serialized as JSON
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_key;
        }
        else if (data.state == wants_key)
        {
            // We're in the attribute list, waiting for the key. Our messages
            // only have one key named "payload"
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 3u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.value != "payload")
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            data.state = wants_value;
        }
        else if (data.state == wants_value)
        {
            // We're in the attribute list, waiting for the value. It contains
            // a JSON payload with the message contents
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.depth != 3u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)

            // Parse payload
            auto jv = boost::json::parse(node.value, ec);
            if (ec)
                CHAT_RETURN_ERROR(ec)
            auto msg = boost::json::try_value_to<redis_wire_message>(jv);
            if (msg.has_error())
                CHAT_RETURN_ERROR(msg.error())
            res.back().messages.push_back(to_message(msg.value(), *data.id));

            // Reset parser state
            data.state = wants_level0_or_entry_list;
            data.id = nullptr;
        }
    }

    // Corrupted response: unfinished message or response
    if (data.state != wants_level0_or_entry_list && data.state != wants_level0_list)
        CHAT_RETURN_ERROR(errc::redis_parse_error)

    return res;
}

result<std::vector<std::string>> chat::parse_batch_xadd_response(node_span nodes)
{
    // Pre-allocate memory
    std::vector<std::string> res;
    res.reserve(nodes.size());

    for (const auto& node : nodes)
    {
        // Verify that the shape of the response matches
        if (node.depth != 0u)
            CHAT_RETURN_ERROR(errc::redis_parse_error)
        else if (node.data_type != resp3::type::blob_string)
            CHAT_RETURN_ERROR(errc::redis_parse_error)

        // Add to response
        res.push_back(node.value);
    }

    return res;
}

result<std::vector<std::string>> chat::parse_string_array_response(node_span nodes)
{
    // Expected shape:
    // depth 0: array
    // depth 1: blob_string items
    if (nodes.empty())
        CHAT_RETURN_ERROR(errc::redis_parse_error)

    const auto& first = nodes.front();
    if (first.depth != 0u || first.data_type != resp3::type::array)
        CHAT_RETURN_ERROR(errc::redis_parse_error)

    std::vector<std::string> res;
    res.reserve(first.aggregate_size);
    for (std::size_t i = 1; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        if (node.depth != 1u)
            continue;
        if (node.data_type != resp3::type::blob_string)
            CHAT_RETURN_ERROR(errc::redis_parse_error)
        res.push_back(node.value);
    }

    return res;
}

result<std::pair<std::vector<std::string>, std::vector<std::string>>> chat::parse_batch_id_pair_array_response(
    node_span nodes
)
{
    // Expected shape for each response:
    // depth 0: array (size 2)
    // depth 1: blob_string (redis_id)
    // depth 1: blob_string (pending_id)
    std::vector<std::string> ids;
    std::vector<std::string> pending_ids;

    std::string tmp_id;
    std::string tmp_pending;
    std::size_t seen_in_current = 0;

    for (const auto& node : nodes)
    {
        if (node.depth == 0u)
        {
            if (node.data_type != resp3::type::array || node.aggregate_size != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            tmp_id.clear();
            tmp_pending.clear();
            seen_in_current = 0;
            continue;
        }

        if (node.depth == 1u)
        {
            if (node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (seen_in_current == 0)
                tmp_id = node.value;
            else if (seen_in_current == 1)
                tmp_pending = node.value;
            ++seen_in_current;

            if (seen_in_current == 2)
            {
                ids.push_back(std::move(tmp_id));
                pending_ids.push_back(std::move(tmp_pending));
                tmp_id.clear();
                tmp_pending.clear();
                seen_in_current = 0;
            }
        }
    }

    if (!tmp_id.empty() || !tmp_pending.empty())
        CHAT_RETURN_ERROR(errc::redis_parse_error)

    return std::pair{std::move(ids), std::move(pending_ids)};
}

result<std::vector<persist_pending_entry>> chat::parse_persist_pending_xrange_response(node_span nodes)
{
    // XRANGE response:
    // array [
    //   [ entry_id, [ field, value, field, value, ... ] ],
    //   ...
    // ]
    std::vector<persist_pending_entry> out;
    if (nodes.empty())
        return out;

    enum state_t
    {
        wants_level0_array,
        wants_entry_array_or_end,
        wants_entry_id,
        wants_kv_array,
        wants_key,
        wants_value
    };

    state_t state = wants_level0_array;
    std::string entry_id;
    std::string room_id;
    std::string redis_id;
    std::string payload;
    std::string current_key;

    auto emit_entry = [&] {
        out.push_back(persist_pending_entry{entry_id, room_id, redis_id, payload});
        entry_id.clear();
        room_id.clear();
        redis_id.clear();
        payload.clear();
        current_key.clear();
    };

    for (const auto& node : nodes)
    {
        switch (state)
        {
        case wants_level0_array:
            if (node.depth != 0u || node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            state = wants_entry_array_or_end;
            break;

        case wants_entry_array_or_end:
            // Each entry is an array at depth 1; if there are no entries, we simply finish.
            if (node.depth != 1u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (node.data_type != resp3::type::array || node.aggregate_size != 2u)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            state = wants_entry_id;
            break;

        case wants_entry_id:
            if (node.depth != 2u || node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            entry_id = node.value;
            state = wants_kv_array;
            break;

        case wants_kv_array:
            if (node.depth != 2u || node.data_type != resp3::type::array)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            state = wants_key;
            break;

        case wants_key:
            if (node.depth != 3u || node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            current_key = node.value;
            state = wants_value;
            break;

        case wants_value:
            if (node.depth != 3u || node.data_type != resp3::type::blob_string)
                CHAT_RETURN_ERROR(errc::redis_parse_error)
            if (current_key == "room_id")
                room_id = node.value;
            else if (current_key == "redis_id")
                redis_id = node.value;
            else if (current_key == "payload")
                payload = node.value;
            current_key.clear();

            // Heuristic: if the next node is at depth 1 (new entry array) we will handle it next
            // iteration after we emit, but we can't see it here. Instead, emit when we see that
            // we are done parsing the kv list: when the next key would be missing we will simply
            // keep alternating key/value until the parser transitions back.
            //
            // Because the node stream includes structure nodes for arrays, we can't reliably know
            // the end-of-kv here without counting. Instead, we remain in wants_key and emit when
            // the next structural node (depth 1 array) is encountered (handled above by starting a
            // new entry) — but we would lose the current entry. So, we emit whenever we transition
            // to a new entry array (handled by seeing depth 1 array while in wants_entry_array_or_end).
            //
            // To make this robust, we emit at the first moment we can: when we see that we are
            // leaving depth 3 (a non-depth-3 node will appear). Since we can't peek, we instead
            // emit when we see the kv array's closing structure node is represented by the next
            // entry array at depth 1. We'll do this by temporarily transitioning to wants_key and
            // allowing the state machine to handle the next node. If the next node is an entry array,
            // wants_key would fail. Therefore, we need a safer approach: rely on aggregate_size.
            //
            // The simplest robust approach is to count key/value pairs using the kv array aggregate_size.
            // Not available here without tracking. Fall back to emitting after we've seen the required
            // fields; this works for our schema (room_id, redis_id, payload).
            if (!entry_id.empty() && !room_id.empty() && !redis_id.empty() && !payload.empty())
            {
                emit_entry();
                state = wants_entry_array_or_end;
            }
            else
            {
                state = wants_key;
            }
            break;
        }
    }

    // If we ended mid-entry, treat as parse error
    if (state != wants_entry_array_or_end)
        CHAT_RETURN_ERROR(errc::redis_parse_error)

    return out;
}

std::string chat::serialize_redis_message(const message& msg)
{
    // Construct the wire message
    redis_wire_message redis_msg{
        msg.id.empty() ? std::nullopt : std::optional<std::string_view>(msg.id),
        msg.content,
        serialize_timestamp(msg.timestamp),
        msg.user_id
    };

    // Serialize it to JSON
    return boost::json::serialize(boost::json::value_from(redis_msg));
}
