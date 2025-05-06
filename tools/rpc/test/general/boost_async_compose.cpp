#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>
#include <thread>
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::thread_pool;
using boost::asio::this_coro::executor;

template<typename Func>
auto async_task(Func f, thread_pool& pool)
  -> awaitable<typename std::invoke_result_t<Func>>
{
  using RetType = typename std::invoke_result_t<Func>;

  // 1) 拿到当前协程的 executor
  auto ex = co_await executor;

  // 2) 挂起协程，在线程池执行 f，然后切回 ex 恢复
  RetType result = co_await boost::asio::async_compose<
      decltype(use_awaitable), void(RetType)>(
    // initiation lambda，相当于 await_suspend()
      [f = std::move(f), &pool, ex](auto& self) mutable 
      {
        // 提交到线程池, 这里可以替换成自己的线程池接口
        boost::asio::post(pool,
          [f = std::move(f), ex, self = std::move(self)]() mutable
            {
              RetType value{};
         
              value = f();  // 执行同步任务
          
          // 任务完成，切回原 executor 恢复协程
              boost::asio::post(ex,
                            [self = std::move(self), value = std::move(value)]() mutable {
                              self.complete(std::move(value));
                            });
            });
      },
      use_awaitable);
  co_return result;
}

awaitable<void> example_coroutine(thread_pool& pool) {
  std::cout << "Coroutine start on thread " << std::this_thread::get_id() << "\n";

  // 提交一个打印任务到线程池，并 co_await 它完成
  co_await async_task([&] {
    std::cout << "Hello from thread pool! thread id=" << std::this_thread::get_id() << "\n";
    return 0;  // 返回任意值，这里不重要
  }, pool);

  std::cout << "Coroutine resumed on thread " << std::this_thread::get_id() << "\n";
}

int main() {
  boost::asio::io_context io;
  thread_pool pool(2);

  // 启动协程
  boost::asio::co_spawn(io,
    [&pool,&io]() -> awaitable<void> {
      co_await example_coroutine(pool);
      io.stop();  // 结束 io_context.run()
    },
    boost::asio::detached);

  io.run();
  pool.join();
  return 0;
}
