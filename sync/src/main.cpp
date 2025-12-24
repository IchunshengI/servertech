#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/index.hpp>
#include <mongocxx/options/update.hpp>
#include <mongocxx/uri.hpp>

#include <json/json.h>
#include <rocketmq/DefaultMQPushConsumer.h>
#include <rocketmq/DefaultMQProducer.h>
#include <rocketmq/MQMessageListener.h>

#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using bsoncxx::builder::basic::document;
using bsoncxx::builder::basic::kvp;
using namespace rocketmq;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

static std::string getenv_or(const char* key, const char* default_value)
{
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : std::string(default_value);
}

static std::int64_t getenv_i64(const char* key, std::int64_t default_value)
{
    const char* v = std::getenv(key);
    if (!v || !*v)
        return default_value;
    try
    {
        return std::stoll(v);
    }
    catch (...)
    {
        return default_value;
    }
}

static bool parse_json(const std::string& body, Json::Value& root)
{
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::string errs;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    const char* begin = body.data();
    const char* end = body.data() + body.size();
    if (!reader->parse(begin, end, &root, &errs))
    {
        std::cerr << "Invalid JSON: " << errs << " body=" << body << "\n";
        return false;
    }
    return true;
}

static ConsumeStatus consume_success()
{
    return static_cast<ConsumeStatus>(0);
}

static ConsumeStatus reconsume_later()
{
    return static_cast<ConsumeStatus>(1);
}

static http::response<http::string_body> make_plain_response(http::status status, std::string body)
{
    http::response<http::string_body> res{status, 11};
    res.set(http::field::content_type, "text/plain; charset=utf-8");
    res.body() = std::move(body);
    res.prepare_payload();
    return res;
}

static http::response<http::string_body> make_json_response(http::status status, const Json::Value& v)
{
    http::response<http::string_body> res{status, 11};
    res.set(http::field::content_type, "application/json; charset=utf-8");
    Json::StreamWriterBuilder w;
    w["indentation"] = "";
    res.body() = Json::writeString(w, v);
    res.prepare_payload();
    return res;
}

static bool redis_dedup_xadd_payload(
    const std::string& host,
    const std::string& port,
    const std::string& stream,
    const std::string& uuid,
    const std::string& payload,
    std::int64_t dedup_ttl_sec,
    std::string& err
)
{
    // Atomically:
    //  - if uuid key exists => return 0 (already inserted)
    //  - else set uuid key with TTL and XADD * payload => return 1
    //
    // We use XADD * to avoid "ID is smaller than stream top" issues on retries/out-of-order.
    static constexpr const char* script = R"lua(
local stream = KEYS[1]
local key = KEYS[2]
local payload = ARGV[1]
local ttl = tonumber(ARGV[2])
if redis.call('SET', key, '1', 'NX', 'EX', ttl) then
  redis.call('XADD', stream, '*', 'payload', payload)
  return 1
end
return 0
)lua";

    try
    {
        asio::io_context ioc{1};
        tcp::resolver resolver{ioc};
        tcp::socket sock{ioc};

        boost::system::error_code ec;
        auto endpoints = resolver.resolve(host, port, ec);
        if (ec)
        {
            err = "resolve failed: " + ec.message();
            return false;
        }

        asio::connect(sock, endpoints, ec);
        if (ec)
        {
            err = "connect failed: " + ec.message();
            return false;
        }

        auto bulk = [](std::string_view s) {
            return "$" + std::to_string(s.size()) + "\r\n" + std::string(s) + "\r\n";
        };

        const std::string key = "dedup_uuid:" + uuid;
        const std::string ttl_s = std::to_string(dedup_ttl_sec);

        // EVAL <script> 2 <stream> <key> <payload> <ttl>
        std::string cmd;
        cmd.reserve(stream.size() + key.size() + payload.size() + ttl_s.size() + 512);
        cmd += "*7\r\n";
        cmd += bulk("EVAL");
        cmd += bulk(script);
        cmd += bulk("2");
        cmd += bulk(stream);
        cmd += bulk(key);
        cmd += bulk(payload);
        cmd += bulk(ttl_s);

        asio::write(sock, asio::buffer(cmd), ec);
        if (ec)
        {
            err = "write failed: " + ec.message();
            return false;
        }

        asio::streambuf buf;
        asio::read_until(sock, buf, "\r\n", ec);
        if (ec)
        {
            err = "read failed: " + ec.message();
            return false;
        }

        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty())
        {
            err = "empty redis response";
            return false;
        }

        if (line[0] == '-')
        {
            err = "redis error: " + line.substr(1);
            return false;
        }

        if (line[0] != ':')
        {
            err = "unexpected redis response: " + line;
            return false;
        }

        // ':' integer response: 1 inserted, 0 already inserted.
        return true;
    }
    catch (const std::exception& e)
    {
        err = std::string("redis exception: ") + e.what();
        return false;
    }
}

class PersistListener final : public MQMessageListener
{
    mongocxx::collection coll_;
    std::string redis_host_;
    std::string redis_port_;
    std::int64_t redis_dedup_ttl_sec_{};
    std::shared_ptr<std::atomic<std::int64_t>> remaining_;
    std::shared_ptr<std::atomic<bool>> stop_;

public:
    PersistListener(
        mongocxx::collection coll,
        std::string redis_host,
        std::string redis_port,
        std::int64_t redis_dedup_ttl_sec,
        std::shared_ptr<std::atomic<std::int64_t>> remaining,
        std::shared_ptr<std::atomic<bool>> stop
    )
        : coll_(std::move(coll)),
          redis_host_(std::move(redis_host)),
          redis_port_(std::move(redis_port)),
          redis_dedup_ttl_sec_(redis_dedup_ttl_sec),
          remaining_(std::move(remaining)),
          stop_(std::move(stop))
    {
    }

    ConsumeStatus consumeMessage(const std::vector<MQMessageExt>& msgs) override
    {
        for (const auto& msg : msgs)
        {
            try
            {
                const std::string body = msg.getBody();

                Json::Value root;
                if (!parse_json(body, root))
                    continue; // skip malformed payloads

                if (!root.isObject())
                {
                    std::cerr << "Invalid JSON payload (not an object): " << body << "\n";
                    continue;
                }

                const auto room_id = root.get("room_id", "").asString();
                const auto uuid = root.get("uuid", "").asString();
                const auto content = root.get("content", "").asString();
                const std::int64_t timestamp = root.get("timestamp", 0).asInt64();
                const std::int64_t user_id = root.get("user_id", 0).asInt64();

                if (room_id.empty() || uuid.empty())
                {
                    std::cerr << "Invalid payload (missing room_id/uuid): " << body << "\n";
                    continue;
                }

                document filter;
                filter.append(kvp("uuid", uuid));

                document set_on_insert;
                set_on_insert.append(kvp("uuid", uuid));
                set_on_insert.append(kvp("room_id", room_id));
                set_on_insert.append(kvp("content", content));
                set_on_insert.append(kvp("timestamp", bsoncxx::types::b_int64{timestamp}));
                set_on_insert.append(kvp("user_id", bsoncxx::types::b_int64{user_id}));

                document update;
                update.append(kvp("$setOnInsert", set_on_insert.extract()));

                mongocxx::options::update opts;
                opts.upsert(true);
                coll_.update_one(filter.view(), update.view(), opts);

                // After MongoDB insert succeeds, write to Redis cache/history (best-effort but retried via MQ).
                Json::Value payload;
                payload["uuid"] = uuid;
                // Preserve the externally-visible message id, even if Redis stream ID is auto-generated.
                payload["id"] = uuid;
                payload["content"] = content;
                payload["timestamp"] = static_cast<Json::Int64>(timestamp);
                payload["user_id"] = static_cast<Json::Int64>(user_id);
                Json::StreamWriterBuilder w;
                w["indentation"] = "";
                const std::string payload_str = Json::writeString(w, payload);

                std::string redis_err;
                if (!redis_dedup_xadd_payload(
                        redis_host_,
                        redis_port_,
                        room_id,
                        uuid,
                        payload_str,
                        redis_dedup_ttl_sec_,
                        redis_err
                    ))
                {
                    std::cerr << "Redis XADD failed, will retry: " << redis_err << "\n";
                    return reconsume_later();
                }

                // Optional: stop after N persisted messages (for a "consume one then exit" workflow).
                if (remaining_ && stop_)
                {
                    const auto r = remaining_->load();
                    if (r > 0)
                    {
                        const auto next = remaining_->fetch_sub(1) - 1;
                        if (next <= 0)
                            stop_->store(true);
                    }
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "Persist failed, will retry: " << e.what() << "\n";
                return reconsume_later();
            }
        }
        return consume_success();
    }
};

static void run_http_server(
    const std::string& bind_addr,
    std::uint16_t port,
    DefaultMQProducer& producer,
    std::mutex& producer_mu,
    const std::string& topic
)
{
    asio::io_context ioc{1};
    const auto address = asio::ip::make_address(bind_addr);
    tcp::acceptor acceptor{ioc, {address, port}};

    std::cout << "HTTP listening on " << bind_addr << ":" << port << "\n";

    for (;;)
    {
        tcp::socket socket{ioc};
        acceptor.accept(socket);

        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        http::response<http::string_body> res = make_plain_response(http::status::not_found, "not found");

        if (req.method() == http::verb::get && req.target() == "/health")
        {
            res = make_plain_response(http::status::ok, "ok");
        }
        else if (req.method() == http::verb::post && req.target() == "/publish")
        {
            Json::Value root;
            if (!parse_json(req.body(), root) || !root.isObject())
            {
                res = make_plain_response(http::status::bad_request, "invalid json");
            }
            else
            {
                const auto room_id = root.get("room_id", "").asString();
                const auto uuid = root.get("uuid", "").asString();
                if (room_id.empty() || uuid.empty())
                {
                    res = make_plain_response(http::status::bad_request, "missing room_id/uuid");
                }
                else
                {
                    const std::string payload = req.body(); // forward as-is
                    try
                    {
                        SendResult send_res;
                        {
                            std::lock_guard<std::mutex> lk(producer_mu);
                            MQMessage msg(topic, "*", payload);
                            send_res = producer.send(msg);
                        }

                        Json::Value out;
                        out["status"] = "ok";
                        out["msg_id"] = send_res.getMsgId();
                        out["send_status"] = static_cast<int>(send_res.getSendStatus());
                        res = make_json_response(http::status::ok, out);
                    }
                    catch (const std::exception& e)
                    {
                        res = make_plain_response(http::status::internal_server_error, std::string("send failed: ") + e.what());
                    }
                }
            }
        }

        res.keep_alive(false);
        http::write(socket, res);

        beast::error_code ec;
        socket.shutdown(tcp::socket::shutdown_send, ec);
    }
}

int main()
{
    const auto namesrv = getenv_or("NAMESRV_ADDR", "rocketmq-servertech:9876");
    const auto topic = getenv_or("MQ_TOPIC", "webchat_message");
    const auto group = getenv_or("MQ_CONSUMER_GROUP", "webchat_message_persist_group");
    const auto producer_group = getenv_or("MQ_PRODUCER_GROUP", "webchat_message_gateway_group");
    const auto http_bind = getenv_or("HTTP_BIND", "0.0.0.0");
    const auto http_port = static_cast<std::uint16_t>(getenv_i64("HTTP_PORT", 8081));
    const auto redis_host = getenv_or("REDIS_HOST", "redis-servertech");
    const auto redis_port = getenv_or("REDIS_PORT", "6379");
    const auto redis_dedup_ttl_sec = getenv_i64("REDIS_DEDUP_TTL_SEC", 30 * 24 * 3600);

    const auto mongo_uri =
        getenv_or("MONGODB_URI", "mongodb://root:root123456@mongodb-servertech:27017/?authSource=admin");
    const auto mongo_db = getenv_or("MONGODB_DB", "chat");
    const auto mongo_collection = getenv_or("MONGODB_COLLECTION", "room_messages");

    static mongocxx::instance instance{};
    mongocxx::client client{mongocxx::uri{mongo_uri}};
    auto db = client[mongo_db];
    auto coll = db[mongo_collection];

    // Idempotency: one message UUID should be inserted once.
    try
    {
        mongocxx::options::index idx_opts;
        idx_opts.unique(true);
        document keys;
        keys.append(kvp("uuid", 1));
        coll.create_index(keys.view(), idx_opts);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Warning: failed to create unique index (uuid): " << e.what() << "\n";
    }

    auto remaining = std::make_shared<std::atomic<std::int64_t>>(getenv_i64("CONSUME_MAX_MESSAGES", 0));
    auto stop = std::make_shared<std::atomic<bool>>(false);

    try
    {
        DefaultMQProducer producer(producer_group);
        producer.setNamesrvAddr(namesrv);
        producer.start();
        std::mutex producer_mu;

        DefaultMQPushConsumer consumer(group);
        consumer.setNamesrvAddr(namesrv);
        consumer.subscribe(topic, "*");
        auto listener =
            std::make_unique<PersistListener>(coll, redis_host, redis_port, redis_dedup_ttl_sec, remaining, stop);
        consumer.registerMessageListener(listener.get());
        consumer.start();
        std::cout << "sync-servertech started (C++). topic=" << topic << " consumer_group=" << group
                  << " producer_group=" << producer_group << " namesrv=" << namesrv << " mongodb=" << mongo_db << "."
                  << mongo_collection << "\n";

        std::thread http_thread([&] {
            run_http_server(http_bind, http_port, producer, producer_mu, topic);
        });

        for (;;)
        {
            if (stop->load())
            {
                consumer.shutdown();
                producer.shutdown();
                std::cout << "sync-servertech stopped (CONSUME_MAX_MESSAGES reached)\n";
                http_thread.detach();
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::seconds(3600));
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to start consumer: " << e.what() << "\n";
        return 1;
    }
}
