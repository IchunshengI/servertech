//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/persist_http_publisher.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/json/serialize.hpp>

#include <chrono>
#include <cstdlib>
#include <string>

using namespace chat;

namespace beast = boost::beast;
namespace http = boost::beast::http;

namespace {

struct persist_http_config
{
    std::string host = "sync-servertech";
    std::string port = "8081";
    std::string target = "/publish";
};

static persist_http_config get_config()
{
    persist_http_config cfg;
    if (const char* v = std::getenv("PERSIST_HTTP_HOST"); v && *v)
        cfg.host = v;
    if (const char* v = std::getenv("PERSIST_HTTP_PORT"); v && *v)
        cfg.port = v;
    if (const char* v = std::getenv("PERSIST_HTTP_TARGET"); v && *v)
        cfg.target = v;
    return cfg;
}

}  // namespace

error_with_message chat::publish_persist_http(
    boost::asio::any_io_executor ex,
    const boost::json::object& payload,
    boost::asio::yield_context yield
)
{
    const auto cfg = get_config();

    boost::asio::ip::tcp::resolver resolver(ex);
    beast::tcp_stream stream(ex);

    boost::system::error_code ec;
    auto endpoints = resolver.async_resolve(cfg.host, cfg.port, yield[ec]);
    if (ec)
        return error_with_message{ec, "persist resolve failed"};

    stream.expires_after(std::chrono::seconds(3));
    stream.async_connect(endpoints, yield[ec]);
    if (ec)
        return error_with_message{ec, "persist connect failed"};

    http::request<http::string_body> req{http::verb::post, cfg.target, 11};
    req.set(http::field::host, cfg.host);
    req.set(http::field::user_agent, "servertech-web-back");
    req.set(http::field::content_type, "application/json; charset=utf-8");
    req.body() = boost::json::serialize(payload);
    req.prepare_payload();
    req.keep_alive(false);

    http::async_write(stream, req, yield[ec]);
    if (ec)
        return error_with_message{ec, "persist write failed"};

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::async_read(stream, buffer, res, yield[ec]);
    if (ec)
        return error_with_message{ec, "persist read failed"};

    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

    if (res.result() != http::status::ok)
        return error_with_message{errc::bad_request, "persist http non-200: " + std::to_string(res.result_int())};

    return {};
}
