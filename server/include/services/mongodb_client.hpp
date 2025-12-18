//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MONGODB_CLIENT_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_MONGODB_CLIENT_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/core/span.hpp>

#include <memory>
#include <string_view>

#include "business_types.hpp"
#include "error.hpp"

namespace chat {

class mongodb_client
{
public:
    virtual ~mongodb_client() {}

    virtual error_with_message store_room_messages(
        std::string_view room_id,
        boost::span<const message> messages
    ) = 0;
};

std::unique_ptr<mongodb_client> create_mongodb_client(boost::asio::any_io_executor ex);

}  // namespace chat

#endif
