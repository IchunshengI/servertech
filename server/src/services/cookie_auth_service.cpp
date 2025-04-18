//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "services/cookie_auth_service.hpp"

#include <algorithm>
#include <iostream>
#include "error.hpp"
#include "services/mysql_client.hpp"
#include "services/redis_client.hpp"
#include "services/session_store.hpp"
#include "util/cookie.hpp"

using namespace chat;
 
namespace http = boost::beast::http;

static constexpr std::string_view session_cookie_name = "sid";
static constexpr std::chrono::seconds session_duration(3600 * 24 * 7);  // 7 days

result_with_message<std::string> cookie_auth_service::generate_session_cookie(
    std::int64_t user_id,
    boost::asio::yield_context yield
)
{
    // Generate a session token
    session_store store{*redis_};
    auto session_id_result = store.generate_session_id(user_id, session_duration, yield);
    if (session_id_result.has_error())
        return std::move(session_id_result).error();

    // Generate the cookie to be set
    return set_cookie_builder(session_cookie_name, *session_id_result)
        .http_only(true)
        .same_site(same_site_t::strict)
        .max_age(session_duration)
        .build_header();
}




/* 从消息中抽取中用于id */
result_with_message<std::pair<std::int64_t,std::string_view>> cookie_auth_service::user_id_from_cookie(
    const boost::beast::http::fields& req,
    boost::asio::yield_context yield
)
{
    // Get the Cookie header from the request
    auto it = req.find(http::field::cookie);
    if (it == req.end())
        CHAT_RETURN_ERROR_WITH_MESSAGE(errc::requires_auth, "")

    // Retrieve the session cookie
    cookie_list cookies(it->value());
    auto cookie_it = std::find_if(cookies.begin(), cookies.end(), [](const cookie_pair& p) {
        return p.name == session_cookie_name;
    });
    if (cookie_it == cookies.end())
        CHAT_RETURN_ERROR_WITH_MESSAGE(errc::requires_auth, "")
    std::cout <<  __FUNCTION__ << " cookie_it->value: " << cookie_it->value << std::endl;
    // Look it up in Redis
    session_store store{*redis_};
    auto result = store.get_user_by_session(cookie_it->value, yield);
    if (result.has_error())
    {
        auto err = std::move(result).error();
        if (err.ec == errc::not_found){
            CHAT_RETURN_ERROR_WITH_MESSAGE(errc::requires_auth, std::move(err.msg));
            // std::cout << "not found" << std::endl;
            // return error_with_message{errc::requires_auth, std::move(err.msg)};
        }
        else
            return err;
    }
    std::cout << __FUNCTION__ << " user_id from reids result.value: " << result.value() << std::endl;
    // std::int64_t user_id = result.value();
    // std::string_view session_value = cookie_it->value;
    return std::make_pair(result.value(),cookie_it->value);
}

result_with_message<std::pair<result_with_message<user>,std::string_view>> cookie_auth_service::user_from_cookie(
    const boost::beast::http::fields& req_headers,
    boost::asio::yield_context yield
)
{
    auto user_id_result = user_id_from_cookie(req_headers, yield);  //通过cookie获取user_id
    if (user_id_result.has_error())
    {
        return std::move(user_id_result).error();
    }
    return std::make_pair(mysql_->get_user_by_id(user_id_result.value().first, yield), user_id_result->second);          //然后通过user_id从MySQL中查询用户名
}
