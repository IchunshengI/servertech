#ifndef SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_THREAD_POLL_HPP
#define SERVERTECHCHAT_SERVER_INCLUDE_SERVICES_THREAD_POLL_HPP

#include <vector>
#include <queue>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <stdexcept>
#include "concurrentqueue.h"
namespace chat{

class ThreadPool{
public:

    explicit ThreadPool(size_t min_size, size_t max_size);

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args... )>::type>{

        using return_type = typename std::result_of<F(Args...)>::type;
        /* 任务封装 */
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        std::function<void()> wrapper = [task]() { (*task)(); }; 
        tasks.enqueue(wrapper);
        condition.notify_one();
        return res;
    }

    ~ThreadPool();
        

private:
    std::vector<std::thread> workers; /* 存储工作线程对象 */
    moodycamel::ConcurrentQueue<std::function<void()>> tasks; /* 无锁任务队列 */

    size_t min_threads_size;            /* 最少运行的线程数量 */
    size_t max_threads_size;            /* 动态扩容的最大限制 */
    std::mutex mutex_;                  /* 线程间同步，用于压入时唤醒 */
    std::condition_variable condition;  /* 同步所需的条件变量 */
    bool stop;                          /* 线程池停止标志 */
};

}

#endif