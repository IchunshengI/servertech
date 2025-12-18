//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/mongodb_client.hpp"

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include <boost/asio/any_io_executor.hpp>

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "timestamp.hpp"

using namespace chat;

namespace {

static std::string getenv_or(std::string_view key, std::string_view default_value)
{
    const char* value = std::getenv(std::string(key).c_str());
    return value ? std::string(value) : std::string(default_value);
}

class mongodb_client_impl final : public mongodb_client
{
    mongocxx::client client_;
    mongocxx::database db_;
    mongocxx::collection room_messages_;

public:
    explicit mongodb_client_impl(std::string uri, std::string db_name)
        : client_(mongocxx::uri(std::move(uri))), db_(client_[std::move(db_name)]), room_messages_(db_["room_messages"])
    {
    }

    error_with_message store_room_messages(std::string_view room_id, boost::span<const message> messages) override final
    {
        try
        {
            std::vector<bsoncxx::document::value> docs;
            docs.reserve(messages.size());

            for (const auto& msg : messages)
            {
                using bsoncxx::builder::basic::document;
                using bsoncxx::builder::basic::kvp;

                document doc;
                doc.append(kvp("room_id", std::string(room_id)));
                doc.append(kvp("redis_id", msg.id));
                doc.append(kvp("content", msg.content));
                doc.append(kvp("timestamp", bsoncxx::types::b_int64{serialize_timestamp(msg.timestamp)}));
                doc.append(kvp("user_id", bsoncxx::types::b_int64{msg.user_id}));
                docs.push_back(doc.extract());
            }

            if (!docs.empty())
                room_messages_.insert_many(std::move(docs));

            return {};
        }
        catch (const mongocxx::exception& e)
        {
            return error_with_message{errc::uncaught_exception, std::string("MongoDB insert failed: ") + e.what()};
        }
        catch (const std::exception& e)
        {
            return error_with_message{errc::uncaught_exception, std::string("MongoDB insert failed: ") + e.what()};
        }
    }
};

}  // namespace

std::unique_ptr<mongodb_client> chat::create_mongodb_client(boost::asio::any_io_executor)
{
    static mongocxx::instance instance{};

    auto uri = getenv_or("MONGODB_URI", "mongodb://root:root123456@mongodb-servertech:27017/?authSource=admin");
    auto db_name = getenv_or("MONGODB_DB", "chat");
    return std::unique_ptr<mongodb_client>{new mongodb_client_impl(std::move(uri), std::move(db_name))};
}
