#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>
#include "log/logger_wrapper.h"

#include "redis_pool.h"
#include "result_with_message.h"
using chat::LOG;

int main(){

  boost::asio::io_context iox;

  auto redis_pool = chat::create_redis_pool(iox);
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
      boost::redis::response<std::string> resp;
     co_await conn->async_exec(req, resp, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
      if (ec){
        std::cerr << "error : " << ec.message();
        co_return;
      }
       // 处理结果
      auto& set_result = std::get<0>(resp);
      if (set_result.has_error()) {
           std::cerr << "error" << std::endl;
      } else {
         std::cerr << set_result.value() << std::endl;
      }
      co_return;
    },
    boost::asio::detached
  );
   iox.run();
//    std::cerr << "hahhahaha" << std::endl;
// auto work = boost::asio::make_work_guard(iox);
//  work.reset(); // 允许 iox 自然停止


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
  return 0;
}