//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "http_session.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <exception>
#include <string_view>
#include <utility>

#include "api/auth.hpp"
#include "api/chat_websocket.hpp"
#include "error.hpp"
#include "request_context.hpp"
#include "shared_state.hpp"
#include "static_files.hpp"

namespace beast = boost::beast;
namespace http = boost::beast::http;
using namespace chat;

static http::message_generator handle_http_request_impl(
    request_context& ctx,
    shared_state& st,
    boost::asio::yield_context yield
)
{   /* 手动解析http的部分 */
    // Attempt to parse the request target
    auto ec = ctx.parse_request_target();
    if (ec)
        return ctx.response().bad_request_text("Invalid request target");
    auto target = ctx.request_target();

    auto segs = target.segments();
    auto method = ctx.request_method();
    if (!segs.empty() && segs.front() == "api")
    {
        // API endpoint. All endpoint handlers have the signature
        // http::message_generator (request_context&, shared_state&, boost::asio::yield_context)
        auto it = std::next(segs.begin());
        auto seg = *it;
        ++it;
        if (seg == "create-account" && it == segs.end())  /* 注册*/
        {
            if (method == http::verb::post)
                return handle_create_account(ctx, st, yield); /* 里面会附带cookie */
            else
                return ctx.response().method_not_allowed();
        }
        else if (seg == "login" && it == segs.end())
        {
            if (method == http::verb::post)
                return handle_login(ctx, st, yield); /* 里面会附带cookie*/
            else
                return ctx.response().method_not_allowed();
        }
        else
        {
            return ctx.response().not_found_text();
        }
    }
    else
    {
        // Static file
        return handle_static_file(ctx, st);
    }
}

static http::message_generator handle_http_request(
    http::request<http::string_body>&& req,
    shared_state& st,
    boost::asio::yield_context yield
)
{
    // Build a request context
    request_context ctx(std::move(req));

    // We don't communicate regular failures using exceptions, but
    // unhandled exceptions shouldn't crash the server.
    try
    {
        return handle_http_request_impl(ctx, st, yield);
    }
    catch (const std::exception& err)
    {
        return ctx.response().internal_server_error(errc::uncaught_exception, err.what());
    }
}

/* 主请求解析 */
void chat::run_http_session(
    boost::asio::ip::tcp::socket&& socket,
    std::shared_ptr<shared_state> state,
    boost::asio::yield_context yield
)
{
    error_code ec;

    // A buffer to read incoming client requests
    boost::beast::flat_buffer buff;

    // A stream allows us to set quality-of-service parameters for the connection,
    // like timeouts.
    boost::beast::tcp_stream stream(std::move(socket));

    while (true)
    {
        // Construct a new parser for each message
        boost::beast::http::request_parser<boost::beast::http::string_body> parser;

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser.body_limit(10000);

        // Set the timeout.
        stream.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream, buff, parser.get(), yield[ec]);    //读取socket数据 已经封装好了的http数据解析

        if (ec == http::error::end_of_stream)
        {
            // This means they closed the connection
            stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            return;
        }
        else if (ec)
        {
            // An unknown error happened
            return log_error(ec, "read");
        }

        // See if it is a WebSocket Upgrade
        if (boost::beast::websocket::is_upgrade(parser.get()))  //判断是不是websocket请求
        {    //如果是webscocket请求,则按照websocket协议进行处理,否则按照http协议进行处理
            // Create a websocket, transferring ownership of the socket
            // and the buffer (we're not using them again here)
            websocket ws(stream.release_socket(), parser.release(), std::move(buff));

            // Perform the session handshake
            ec = ws.accept(yield);
            if (ec)
                return log_error(ec, "websocket accept");

            // Run the websocket session. This will run until the client
            // closes the connection or an error occurs.
            // We don't use exceptions to communicate regular failures, but an
            // unhandled exception in a websocket session shoudn't crash the server.
            try
            {
                auto err = handle_chat_websocket(std::move(ws), state, yield);
                if (err.ec && err.ec != boost::beast::websocket::error::closed)
                    log_error(err, "Running chat websocket session");
            }
            catch (const std::exception& err)
            {
                log_error(
                    errc::uncaught_exception,
                    "Uncaught exception while running websocket session",
                    err.what()
                );
            }
            return;
        }
        //到这里说明不是websocket请求,而是http请求
        // It's a regular HTTP request.
        // Attempt to serve it and generate a response
        /* 生成cookie也是在这个阶段生成的 */
        http::message_generator msg = handle_http_request(parser.release(), *state, yield); //处理http请求

        // Determine if we should close the connection
        bool keep_alive = msg.keep_alive();

        // Send the response
        /* 异步心跳相应发送 */
        beast::async_write(stream, std::move(msg), yield[ec]);
        if (ec)
            return log_error(ec, "write");

        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        if (!keep_alive)
        {
            stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            return;
        }
    }
}
