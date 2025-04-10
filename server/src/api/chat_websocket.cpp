//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api/chat_websocket.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/core/span.hpp>
#include <boost/variant2/variant.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>

#include "api/api_types.hpp"
#include "business_types.hpp"
#include "error.hpp"
#include "services/cookie_auth_service.hpp"
#include "services/pubsub_service.hpp"
#include "services/redis_client.hpp"
#include "services/room_history_service.hpp"
#include "shared_state.hpp"
#include "util/websocket.hpp"
#include <boost/beast/websocket/error.hpp>
using namespace chat;

namespace {


 
// Rooms are static for now.
// static constexpr std::array<std::string_view, 4> room_ids{
//     "beast",
//     "async",
//     "db",
//     "wasm",
// };

// static constexpr std::array<std::string_view, room_ids.size()> room_names{
//     "程序员老廖",
//     "Boost.Async",
//     "Database connectors",
//     "Web assembly",
// };


// Rooms are static for now.
static constexpr std::array<std::string_view, 2> room_ids{
    "test_channe1",
    "test_channe2",
};

static constexpr std::array<std::string_view, room_ids.size()> room_names{
    "内测频道1",
    "内测频道2",
};

// An owning type containing data for the hello event.
struct hello_data
{
    std::vector<room> rooms;
    username_map usernames;
};

// Retrieves the data required to send the hello event
static result_with_message<hello_data> get_hello_data(shared_state& st, boost::asio::yield_context yield)
{
    // Retrieve room history
    room_history_service history_service(st.redis(), st.mysql());
    auto history_result = history_service.get_room_history(room_ids, yield);
    if (history_result.has_error())
        return std::move(history_result).error();
    assert(history_result->first.size() == room_ids.size());

    // Compose hello data
    hello_data res{{}, std::move(history_result->second)};
    res.rooms.reserve(room_ids.size());
    for (std::size_t i = 0; i < room_ids.size(); ++i)
    {
        res.rooms.push_back(
            room{std::string(room_ids[i]), std::string(room_names[i]), std::move(history_result->first[i])}
        );
    }

    return res;
}

struct event_handler_visitor
{
    const user& current_user;
    websocket& ws;
    shared_state& st;
    boost::asio::yield_context yield;

    // Parsing error
    error_with_message operator()(error_code ec) const noexcept { return error_with_message{ec}; }

    // Messages event
    error_with_message operator()(client_messages_event& evt) const
    {
        // Set the timestamp
        auto timestamp = timestamp_t::clock::now();

        // Compose a message array
        std::vector<message> msgs;
        msgs.reserve(evt.messages.size());
        for (auto& msg : evt.messages)
        {
            msgs.push_back(message{ //1.先把消息存储到std::vector<message> msgs;
                "",  // blank ID, will be assigned by Redis
                std::move(msg.content),
                timestamp,
                current_user.id,
            });
        }

        // Store it in Redis  2. 把消息存储到redis，使用redis的stream类型 参考：https://www.runoob.com/redis/redis-stream.html
        auto ids_result = st.redis().store_messages(evt.roomId, msgs, yield); //返回消息id
        if (ids_result.has_error())
            return std::move(ids_result).error();
        auto& ids = ids_result.value(); //获取到消息id

        // Set the message IDs appropriately
        assert(msgs.size() == ids.size());
        for (std::size_t i = 0; i < msgs.size(); ++i)
            msgs[i].id = std::move(ids[i]);     //更新消息id

        // Compose a server_messages event with all data we have
        server_messages_event server_evt{evt.roomId, current_user, msgs};   //封装消息

        // Broadcast the event to all clients
        // ? 这里的广播是怎么回事？ 发布-订阅者模式？
        st.pubsub().publish(evt.roomId, server_evt.to_json());  //发送给所有的客户端
        return {};
    }

    // Request room history event
    error_with_message operator()(chat::request_room_history_event& evt) const
    {
        // Get room history
        room_history_service svc(st.redis(), st.mysql());
        auto history = svc.get_room_history(evt.roomId, yield);
        if (history.has_error())
            return std::move(history).error();

        // Compose a room_history event
        chat::room_history_event response_evt{evt.roomId, history->first, history->second};
        auto payload = response_evt.to_json();

        // Send it
        chat::error_code ec;
        ws.write(payload, yield[ec]);
        return {ec};
    }

    error_with_message operator()(const client_exit_event& evt) const // 1. 参数改为 const 引用
    {
        boost::system::error_code ec;
        
        // 主动关闭 WebSocket 连接 (使用结构体成员变量名 ws 和 yield)
        ws.close(boost::beast::websocket::close_code::normal, yield[ec]); // 2. 修正变量名为 ws 和 yield
        if (ec)
        {
            // 使用 error_with_message 返回错误 (保持与其它函数相同的返回类型)
            return error_with_message{ec, "Failed to close WebSocket"};
        }

        /* 这里要删除对应的cookie哦 */


        // 返回特定错误码表示客户端主动退出 (使用现有错误码类型)
        return error_with_message{
            boost::beast::websocket::error::closed, // 错误码
            "Client exited normally"                // 消息
        };
    }

};

// Messages are broadcast between sessions using the pubsub_service.
// We must implement the message_subscriber interface to use it.
// Each websocket session becomes a subscriber.
// We use room IDs as topic IDs, and websocket message payloads as subscription messages.
class chat_websocket_session final : public message_subscriber,
                                     public std::enable_shared_from_this<chat_websocket_session>
{
    websocket ws_;
    std::shared_ptr<shared_state> st_;
    std::string_view cookie_; /* 当前会话存放的cookie信息 */
public:
    chat_websocket_session(websocket socket, std::shared_ptr<shared_state> state) noexcept
        : ws_(std::move(socket)), st_(std::move(state)), cookie_()
    {
    }

    // Subscriber callback
    void on_message(std::string_view serialized_message, boost::asio::yield_context yield) override final
    {
        ws_.write(serialized_message, yield);
    }

    /* 
        每个客户端在服务器都有这个一个对应的session，每个session都有一个对应的协程
    */
    // Runs the session until completion
    error_with_message run(boost::asio::yield_context yield)
    {
        error_code ec;

        // Check that the user is authenticated
        auto result = st_->cookie_auth().user_from_cookie(ws_.upgrade_request(), yield);   //读取cookie中的用户信息
        if(result.has_error()){
            ws_.close(boost::beast::websocket::policy_error, yield);
            return result.error();
        }
        auto user_result = result->first;
        auto user_cookie = result->second;
        if (user_result.has_error())
        {
            // If it's not, close the websocket. This is the preferred approach
            // when checking authentication in websockets, as opposed to failing
            // the websocket upgrade, since the client doesn't have access to
            // upgrade failure information.
            log_error(user_result.error(), "Websocket authentication failed");
            ws_.close(boost::beast::websocket::policy_error, yield);  // Ignore the result
            return {};
        }
        const auto& current_user = user_result.value();

        // Lock writes in the websocket. This ensures that no message is written before the hello.
        auto write_guard = ws_.lock_writes(yield);

        // Subscribe to messages for the available rooms
        auto pubsub_guard = st_->pubsub().subscribe_guarded(shared_from_this(), room_ids);

        // Retrieve the data required for the hello message  Hello消息其实也就是房间的历史消息
        auto hello_data = get_hello_data(*st_, yield);  //获取房间的历史消息
        if (hello_data.has_error())
            return hello_data.error();

        // Compose the hello event and write it
        hello_event hello_evt{current_user, hello_data->rooms, hello_data->usernames};  //websocket刚连接时，封装hello消息
        auto serialized_hello = hello_evt.to_json();
        std::cout << __FUNCTION__ << " send hello: " << serialized_hello << std::endl;
        ec = ws_.write_locked(serialized_hello, write_guard, yield);  // 发送hello消息
        if (ec)
            return {ec};

        // Once the hello is sent, we can start sending messages through the websocket
        write_guard.reset();

        // Read subsequent messages from the websocket and dispatch them
        while (true)        //在这个循环里不断读取客户端发送过来的消息，然后再转发给其他客户端
        {
            // Read a message
            auto raw_msg = ws_.read(yield); // 1. 读取消息
            if (raw_msg.has_error())
                return {raw_msg.error()};

            // Deserialize it   解析json数据
            /* 这里其实就可以丢给线程池来做了 */
            auto msg = chat::parse_client_event(raw_msg.value());  // 2. 通过解析json数据解析客户端发送的消息

            // Dispatch
            auto err = boost::variant2::visit(event_handler_visitor{current_user, ws_, *st_, yield}, msg); // 处理客户端发送的消息
            if (err.ec){
                if(err.ec == boost::beast::websocket::error::closed){
                    /* 删除对应的缓存哩 */
                    constexpr std::string_view prefix = "session_";
                    std::string res;
                    res.reserve(prefix.size() + user_cookie.size());
                    res += prefix;
                    res += user_cookie;
                    auto err_ = st_->redis().delete_key(res, yield);
                    if(err_.ec){
                        return err_;
                    }
                }
                return err;
            }
        }
    }
};

}  // namespace

error_with_message chat::handle_chat_websocket(
    websocket socket,
    std::shared_ptr<shared_state> state,
    boost::asio::yield_context yield
)
{
    auto sess = std::make_shared<chat_websocket_session>(std::move(socket), std::move(state));
    return sess->run(yield);
}