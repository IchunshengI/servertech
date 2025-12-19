//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_PERSIST_HTTP_PUBLISHER_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_PERSIST_HTTP_PUBLISHER_HPP

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>

#include "error.hpp"

namespace chat {

// Publishes a persistence request to the sync service via HTTP.
// Best-effort; returns an error on transport errors or non-200 status.
error_with_message publish_persist_http(
    boost::asio::any_io_executor ex,
    const boost::json::object& payload,
    boost::asio::yield_context yield
);

}  // namespace chat

#endif
