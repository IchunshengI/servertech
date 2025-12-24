//
// Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "api/chat_websocket.hpp"
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/core/span.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>

#include "api/api_types.hpp"
#include "business_types.hpp"
#include "error.hpp"
#include "rooms.hpp"
#include "services/cookie_auth_service.hpp"
#include "services/persist_http_publisher.hpp"
#include "services/pubsub_service.hpp"
#include "services/redis_client.hpp"
#include "services/room_history_service.hpp"
#include "shared_state.hpp"
#include "timestamp.hpp"
#include "util/websocket.hpp"
#include <boost/beast/websocket/error.hpp>
#include "rpc_client.h"
using namespace chat;

namespace {

static std::array<unsigned char, 6> get_uuid_v1_node_id()
{
    static std::once_flag once;
    static std::array<unsigned char, 6> cached{};

    std::call_once(once, [] {
        auto hexval = [](char c) -> int {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F')
                return 10 + (c - 'A');
            return -1;
        };

        auto parse_mac = [&](const std::string& s, std::array<unsigned char, 6>& out) -> bool {
            // Expected: "aa:bb:cc:dd:ee:ff"
            if (s.size() < 17)
                return false;
            for (int i = 0; i < 6; ++i)
            {
                const std::size_t pos = static_cast<std::size_t>(i * 3);
                const int hi = hexval(s[pos]);
                const int lo = hexval(s[pos + 1]);
                if (hi < 0 || lo < 0)
                    return false;
                out[i] = static_cast<unsigned char>((hi << 4) | lo);
                if (i < 5 && s[pos + 2] != ':')
                    return false;
            }
            return true;
        };

        // Best-effort MAC retrieval on Linux via sysfs.
        // If not available, fall back to a random node id (RFC 4122 sets multicast bit to 1).
        try
        {
            for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net"))
            {
                const auto ifname = entry.path().filename().string();
                if (ifname == "lo")
                    continue;
                std::ifstream in(entry.path() / "address");
                if (!in)
                    continue;
                std::string mac;
                std::getline(in, mac);
                std::array<unsigned char, 6> node{};
                if (parse_mac(mac, node))
                {
                    bool all_zero = true;
                    for (auto b : node)
                        all_zero = all_zero && (b == 0);
                    if (!all_zero)
                    {
                        cached = node;
                        return;
                    }
                }
            }
        }
        catch (...)
        {
        }

        std::random_device rd;
        for (auto& b : cached)
            b = static_cast<unsigned char>(rd());
        cached[0] |= 0x01; // multicast bit => random node id
    });

    return cached;
}

static std::string make_uuid_v1()
{
    // UUID v1 (RFC 4122): time-based (100ns intervals since 1582-10-15) + clock sequence + node id (MAC).
    // We use MAC when available; otherwise we generate a random node id.
    static std::mutex mu;
    static std::uint64_t last_ts_100ns = 0;
    static std::uint16_t clock_seq = 0;
    static bool clock_seq_init = false;

    // 100ns ticks since UUID epoch.
    constexpr std::uint64_t k_uuid_epoch_offset_100ns = 0x01B21DD213814000ULL; // 1582-10-15 to 1970-01-01
    const auto now = std::chrono::system_clock::now();
    const auto since_unix = now.time_since_epoch();
    const auto now_100ns =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(since_unix).count() / 100);

    std::uint64_t ts;
    {
        std::lock_guard<std::mutex> lk(mu);
        ts = now_100ns + k_uuid_epoch_offset_100ns;

        if (!clock_seq_init)
        {
            std::random_device rd;
            clock_seq = static_cast<std::uint16_t>(rd()) & 0x3FFF;
            clock_seq_init = true;
        }

        if (ts <= last_ts_100ns)
            clock_seq = static_cast<std::uint16_t>((clock_seq + 1) & 0x3FFF);
        last_ts_100ns = ts;
    }

    const std::uint32_t time_low = static_cast<std::uint32_t>(ts & 0xFFFFFFFFULL);
    const std::uint16_t time_mid = static_cast<std::uint16_t>((ts >> 32) & 0xFFFFULL);
    const std::uint16_t time_hi = static_cast<std::uint16_t>((ts >> 48) & 0x0FFFULL);
    const std::uint16_t time_hi_and_version = static_cast<std::uint16_t>(time_hi | 0x1000U); // version 1

    const std::uint8_t clk_seq_hi = static_cast<std::uint8_t>(((clock_seq >> 8) & 0x3FU) | 0x80U); // variant 1
    const std::uint8_t clk_seq_lo = static_cast<std::uint8_t>(clock_seq & 0xFFU);

    const auto node = get_uuid_v1_node_id();

    std::array<unsigned char, 16> bytes{};
    bytes[0] = static_cast<unsigned char>((time_low >> 24) & 0xFF);
    bytes[1] = static_cast<unsigned char>((time_low >> 16) & 0xFF);
    bytes[2] = static_cast<unsigned char>((time_low >> 8) & 0xFF);
    bytes[3] = static_cast<unsigned char>((time_low)&0xFF);
    bytes[4] = static_cast<unsigned char>((time_mid >> 8) & 0xFF);
    bytes[5] = static_cast<unsigned char>((time_mid)&0xFF);
    bytes[6] = static_cast<unsigned char>((time_hi_and_version >> 8) & 0xFF);
    bytes[7] = static_cast<unsigned char>((time_hi_and_version)&0xFF);
    bytes[8] = clk_seq_hi;
    bytes[9] = clk_seq_lo;
    for (int i = 0; i < 6; ++i)
        bytes[10 + i] = node[i];

    auto hex = [](unsigned char v) -> char {
        static constexpr char k[] = "0123456789abcdef";
        return k[v & 0x0F];
    };

    std::string out;
    out.reserve(36);
    for (int i = 0; i < 16; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out.push_back('-');
        out.push_back(hex(bytes[i] >> 4));
        out.push_back(hex(bytes[i]));
    }
    return out;
}

// Publishes messages to the sync service (MQ gateway) via HTTP.
// This is a required step before broadcasting to clients.
static error_with_message publish_messages_via_http_required(
    boost::asio::any_io_executor ex,
    std::string_view room_id,
    std::vector<message>& messages,
    boost::asio::yield_context yield
)
{
    for (std::size_t i = 0; i < messages.size(); ++i)
    {
        const auto& msg = messages[i];

        boost::json::object payload;
        payload["room_id"] = room_id;
        payload["uuid"] = msg.id; // uuid as the message id + MQ idempotency key
        payload["content"] = msg.content;
        payload["timestamp"] = serialize_timestamp(msg.timestamp);
        payload["user_id"] = msg.user_id;

        if (auto err = publish_persist_http(ex, payload, yield); err.ec)
            return err;
    }

    return {};
}


 
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
                make_uuid_v1(),
                std::move(msg.content),
                timestamp,
                current_user.id,
            });
        }

        // Required: publish to sync service (MQ gateway). Only broadcast if this succeeds.
        if (auto err = publish_messages_via_http_required(ws.get_executor(), evt.roomId, msgs, yield); err.ec)
        {
            log_error(err, "persist http");
            return err;
        }

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


    // Session event
    error_with_message operator()(client_session_messages_event& evt) const
    {
        // Set the timestamp
        auto timestamp = timestamp_t::clock::now();

        // Compose a message array
        std::vector<message> msgs;
        msgs.reserve(evt.messages.size());
        
        
        std::cout << "\n\nsession id is : " << evt.sessionId << std::endl;
        std::cout << " size : " << evt.messages.size()<< std::endl;;
        for (auto& msg : evt.messages)
        {
            msgs.push_back(message{ //1.先把消息存储到std::vector<message> msgs;
                "",  // blank ID, will be assigned by Redis
                std::move(msg.content),
                timestamp,
                current_user.id,
            });
        }
       
       auto query = msgs[0].content;
       std::shared_ptr<websocket> ws_prt(&ws,[](websocket*){}); // 只是单纯使用引用，不释放真正的内存
       std::string session_id = evt.sessionId;
       const auto user_id = static_cast<std::uint32_t>(current_user.id);
       std::uint32_t session_id_num = 0;
       try
       {
           session_id_num = static_cast<std::uint32_t>(std::stoul(session_id));
       }
       catch (...)
       {
           session_id_num = 0;
       }
       //websocket& socket    = ws; 
       boost::asio::co_spawn(ws.get_executor(), [ws_prt, query, session_id, user_id, session_id_num]()-> boost::asio::awaitable<void>
       {


	          static constexpr const char* k_ai_api_key = "sk-c5611c6c9cac4d359e857ce63ae5a274";
	          const std::string hash_key = std::to_string(user_id) + ":" + std::to_string(session_id_num);
	          rpc::RpcClient rpc_client_with_token(ws_prt->get_executor(), std::string(k_ai_api_key), hash_key);
	          auto result = co_await rpc_client_with_token.Query(query);
	           if (result.has_error()){
	              std::cerr << "错误 !" << std::endl;
	              co_return;
	           }
          static const user k_root_user{0, " 陆零妖哔"};
          message msg{
              "",                      // 空 ID（可选，如果 Redis 分配就留空）
              std::move(result.value()),       // 消息内容
              timestamp_t::clock::now(), // 当前时间戳
              k_root_user.id           // user_id 是 Root（固定为 0）
          };

          // 3. 组织为一个 vector，传 span
          std::vector<message> messages;
          messages.clear();
          messages.push_back(std::move(msg));

          // 4. 构造 event
          server_update_session_event send_evt{
              .session_id = session_id,  // 你要发给哪个 session
              .sending_user = k_root_user,
              .messages = messages              // 用 span 传进去
          };

          // // 5. 序列化为 JSON
          std::string json = send_evt.to_json();
          // //std::string ddd = "dada";
          co_await ws_prt->write(json);
          co_return;
       },
       boost::asio::detached
       );
        /* 这里目前只判断一条 */
      //  auto query = msgs[0].content;
      //  /* 这里直接挂起这个协程吧？重新起一条协程去执行rpc远程调用？然后执行完的话就恢复这个协程 */
      //  /* 挂起这个协程的话，接收怎么办？ */
      //  /* 直接起一个协程,无栈协程去执行rpc */
      //  /* 获取返回结果后，再起一个有栈协程*/
      // static const user k_root_user{0, "Root"};
      // static int count = 1;
      // std::string content = "hello " + std::to_string(count);
      // std::cout << content << std::endl;
      // count++;
      // message msg{
      //     "",                      // 空 ID（可选，如果 Redis 分配就留空）
      //     std::move(content),       // 消息内容
      //     timestamp_t::clock::now(), // 当前时间戳
      //     k_root_user.id           // user_id 是 Root（固定为 0）
      // };
      // // 3. 组织为一个 vector，传 span
      // std::vector<message> messages;
      // messages.clear();
      // messages.push_back(std::move(msg));

      // // 4. 构造 event
      // server_update_session_event send_evt{
      //     .session_id = evt.sessionId,  // 你要发给哪个 session
      //     .sending_user = k_root_user,
      //     .messages = messages              // 用 span 传进去
      // };

      // // 5. 序列化为 JSON
      // std::string json = send_evt.to_json();
      // //std::string ddd = "dada";
      // ws.write(json, yield);
        return {};
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
