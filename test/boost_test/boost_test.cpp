#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

namespace asio = boost::asio;

int main() {
    asio::io_context ioc;
    int counter = 0;

    // 创建协程（关键修改：使用真正的异步操作）
    asio::spawn(ioc, [&](asio::yield_context yield) {
        asio::steady_timer timer(ioc);
        for (int i = 0; i < 100; ++i) {
            // 使用定时器制造异步点
            timer.expires_after(std::chrono::milliseconds(10));
            timer.async_wait(yield);

            int current = ++counter;
            std::cout << "Counter: " << current 
                      << ", Thread ID: " 
                      << std::this_thread::get_id() 
                      << std::endl;
        }
    }, asio::detached);

    // 创建4个工作线程
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&ioc] {
            try {
                std::cout << "Worker started: " 
                          << std::this_thread::get_id() 
                          << std::endl;
                ioc.run();
                std::cout << "Worker exited: " 
                          << std::this_thread::get_id() 
                          << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    return 0;
}