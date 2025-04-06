#include <mutex>
#include <thread>
#include <vector>

#include <iostream>

std::mutex mutex_;
int i = 0;
void fun()
{
     
    while (true){
        std::lock_guard<std::mutex> lock(mutex_);
        if(i < 20000){
            i++;
        }else{
            return;
        }
     }
}

int main(){
 std::vector<std::thread> threads;
    // 创建 3 个线程
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back(fun);
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 主线程读取最终值
    std::cout << "Final value of i: " << i << std::endl;

}