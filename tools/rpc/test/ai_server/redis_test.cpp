#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstdlib>
#include "log/logger_wrapper.h"

#include "redis_pool.h"
#include "result_with_message.h"
using rpc::result_with_message;
using rpc::error_with_message;
using chat::LOG;

int main(){
//ai_server_CSyr6q4GQ0oqNiQHu55sTA==
  boost::asio::io_context iox;


  /* 
    redis 存值测试
 */
  auto redis_pool = chat::create_redis_pool(iox.get_executor());
  boost::asio::co_spawn(
    iox,
    [&]() -> boost::asio::awaitable<void> {
      boost::system::error_code ec;
      auto result = co_await redis_pool->GetConnection();
      if (result.has_error())
      {
        std::cerr << "error" << std::endl;
        co_return;
      }

      auto& conn = result.value();

      //std::cout << "地址为 " << conn.get()<< std::endl;
      boost::redis::request req;
      req.push("SET", "test_key", "Hello Redis!");
      boost::redis::ignore_t resp; 
      //boost::redis::response<std::string> resp;
     co_await conn->async_exec(req, resp, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
      if (ec){
        std::cerr << "error : " << ec.message();
        co_return;
      }

      co_return;
    },
    boost::asio::detached
  );
  //  iox.run();



//    boost::redis::connection conn_(iox);
//    boost::redis::config cfg;

//     boost::asio::co_spawn(
//     iox,
//     [&]() -> boost::asio::awaitable<void> {
//        std::cerr << "hello" << std::endl;

//       cfg.addr.host = "127.0.0.1";
//       cfg.addr.port = "6379";
//       boost::system::error_code ec;
//       conn_.async_run(cfg, {},boost::asio::detached);

//       // 循环发送 PING 直到成功
//       while (true) {
//           boost::redis::request req;
//           req.push("PING", "ConnectionTest");
          
//           boost::redis::response<std::string> resp;
//           co_await conn_.async_exec(req, resp, boost::asio::use_awaitable);

//           auto& pong = std::get<0>(resp);
//           if (!pong.has_error() && *pong == "ConnectionTest") {
//               std::cout << "连接成功!" << std::endl;
//               break;
//           }

//           std::cerr << "等待连接..." << std::endl;
//           boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
//           timer.expires_after(std::chrono::seconds(1));
//           co_await timer.async_wait(boost::asio::use_awaitable);
//       }


//       boost::redis::request req;
//       req.push("SET", "test_key", "Hello Redis!");
//       boost::redis::response<std::string> resp;
//       std::cerr << "hello" << std::endl;
//       co_await conn_.async_exec(req, resp, boost::asio::use_awaitable);
//        // 处理结果
//        auto& set_result = std::get<0>(resp);
//       if (set_result.has_error()) {
//            std::cerr << "error" << std::endl;
//       } else {
//          std::cerr << set_result.value() << std::endl;
//       }
//       conn_.cancel();
//       co_return;
//     },
//     boost::asio::detached
//   );

//  iox.run();


  /* 
  redis 哈希取值测试 
 */
  boost::asio::co_spawn(
    iox,
    [&]() -> boost::asio::awaitable<void> {
      boost::system::error_code ec;
      auto result = co_await redis_pool->GetConnection();
      if (result.has_error())
      {
        std::cerr << "error" << std::endl;
        co_return;
      }
      std::string redis_key = "xx";
      auto& conn = result.value();
      boost::redis::request req;
      //boost::redis::response<std::map<std::string, std::string>> resp;  // 存储返回的哈希数据
      boost::redis::response<std::int64_t, std::map<std::string, std::string>> resp;
      req.push("EXISTS", redis_key);
      req.push("HGETALL", redis_key);

      co_await conn->async_exec(req, resp, boost::asio::redirect_error(boost::asio::use_awaitable, ec));

      if (ec) {
        LOG("Error") << "redis async_exec error: " << ec.message();
        co_return ;
      }

      auto key_exists = std::get<0>(resp);  // EXISTS 返回 0/1
      if (key_exists == 0) {
        std::cerr << "session key not found in redis" << std::endl;
        co_return;
      }

      const auto& hash = std::get<1>(resp);
      std::string api_key_    = hash->at("api_key");
      std::cerr << "api_key is : " << api_key_ << std::endl;
      co_return;
    },
    boost::asio::detached
  );
   iox.run();
  return 0;
}