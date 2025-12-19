//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_SERIALIZATION_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_REDIS_SERIALIZATION_HPP

#include <boost/core/span.hpp>
#include <boost/redis/resp3/node.hpp>

#include <string>
#include <vector>

#include "business_types.hpp"
#include "error.hpp"

// Contains function definitions to parse Redis responses. Used to
// implement redis_client. Boost.Redis doesn't support streams out
// of the box, so the parsing is non-trivial

namespace chat {

using node_span = boost::span<const boost::redis::resp3::node>;

// Parses the result of getting the history for several rooms, in a batch
// (i.e. several batched XREVRANGEs)
result<std::vector<message_batch>> parse_room_history_batch(node_span from);

// Parses the response of a batch of XADDs. The response of each XADD is a string,
// containing the ID of the inserted record. Calling execute with vector<string>
// doesn't work because Boost.Redis will attempt to parse a single response containing
// an array of strings, instead of multiple responses with a single string
result<std::vector<std::string>> parse_batch_xadd_response(node_span from);

// Parses the response of a Redis EVAL returning a single top-level array
// of strings (used for Lua scripts).
result<std::vector<std::string>> parse_string_array_response(node_span from);

// Parses the result of batched Lua EVAL calls, where each EVAL returns a
// 2-element array: [redis_id, pending_id].
result<std::pair<std::vector<std::string>, std::vector<std::string>>> parse_batch_id_pair_array_response(
    node_span from
);

// Entry in the persist_pending stream (used for persistence retries).
struct persist_pending_entry
{
    std::string entry_id;
    std::string room_id;
    std::string redis_id;
    std::string payload;
};

// Parses the response of:
//   XRANGE persist_pending - + COUNT N
// into a list of entries, preserving order.
result<std::vector<persist_pending_entry>> parse_persist_pending_xrange_response(node_span from);

// We store messages in streams as serialized JSON objects. Serialize
// a message into its JSON representation
std::string serialize_redis_message(const message& msg);

}  // namespace chat

#endif
