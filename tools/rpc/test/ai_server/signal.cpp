
// SimpleSignal.hpp
// 一个可在协程间用 async/await 同步的简单信号类
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <functional>

namespace util {

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::use_awaitable_t;
class SimpleSignal {
public:
  explicit SimpleSignal(boost::asio::any_io_executor ex)
    : ex_(ex), signaled_(false) {}

  awaitable<void> Wait() {
    auto token = use_awaitable;  // 非 const，用于 async_compose

    return boost::asio::async_compose<use_awaitable_t<>, void()>(
      // initiation
      [this](auto& self) mutable {
        if (signaled_) {
          // 信号已发 → 立即唤醒
          boost::asio::post(ex_, [&self]() mutable {
            self.complete();
          });
        } else {
          // 还未发信号 → 保存 handler 到 shared_ptr 中
          auto sp = std::make_shared<std::decay_t<decltype(self)>>(std::move(self));
          complete_ = [sp, ex = ex_]() mutable {
            boost::asio::post(ex, [sp]() mutable {
              sp->complete();
            });
          };
        }
      },
      token,
      ex_);
  }

  void Signal() {
    if (signaled_) return;
    signaled_ = true;
    if (complete_) {
      complete_();  // 如果有人在等，就唤醒
    }
  }

  void Reset()
  {
    signaled_ = false;
  }



private:
  boost::asio::any_io_executor ex_;
  std::function<void()>        complete_;
  std::atomic<bool>            signaled_;
};

} // namespace util


/* 

声明
auto signal = std::make_shared<SimpleSignal>(iox.get_executor());
调用测
signal->signal();

等待测
co_await signal->async_wait();


*/