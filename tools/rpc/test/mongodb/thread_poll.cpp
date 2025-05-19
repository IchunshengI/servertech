#include "thread_poll.hpp"

namespace chat{


/* 初始构造函数 */
ThreadPool::ThreadPool(size_t min_size, size_t max_size) : stop(false), min_threads_size(min_size), max_threads_size(max_size){
    if(min_threads_size == 0){
        min_threads_size = 1;
    }

    for(size_t i=0; i < min_threads_size; ++i){
        workers.emplace_back([this](){
            for(;;){
                std::function<void()> task = nullptr;
                tasks.try_dequeue(task);
                
                while(!task){
                    if(this->stop) return; 
                    { /* RAII */
                        std::unique_lock<std::mutex> lock(this->mutex_);
                        this->condition.wait(lock);
                    }
                    tasks.try_dequeue(task);
                }
                task();
            }
        });
    }
}


ThreadPool::~ThreadPool(){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop = true;
    }
    condition.notify_all();
    for(std::thread& worker: workers){
        worker.join();
    }
}

}

