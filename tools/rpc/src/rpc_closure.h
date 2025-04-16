// rpc_closure.h 最终版本
#ifndef RPC_CLOSURE_H
#define RPC_CLOSURE_H

#include "google/protobuf/service.h"
#include <functional>
#include <memory>

namespace rpc {

class RpcClosure : public google::protobuf::Closure {
public:
    // 使用工厂方法创建实例
    static RpcClosure* Create(std::function<void()> callback) {
        return new RpcClosure(std::move(callback));
    }

    void Run() override {
        if (callback_) {
            callback_(); // 执行用户回调
        }
        delete this;    // 遵循Protobuf Closure生命周期
    }

private:
    explicit RpcClosure(std::function<void()> callback)
        : callback_(std::move(callback)) {}

    std::function<void()> callback_;
};

} // namespace rpc

#endif