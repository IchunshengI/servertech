/*
	Date	  :	2025年4月14日
	Author	:	chunsheng
	Brief		:	协程信息返回通用文件，用于判断是否出现了执行错误
*/

#ifndef RPC_RESULT_WITH_MESSAGE_H
#define RPC_RESULT_WITH_MESSAGE_H

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/result.hpp>
#include <string>
#include <iostream>

namespace rpc{

enum class errc {
    not_found = 1,
    http_error,
    redis_runtime_error,
    nums_error,
    invalid_base64,
    rpc_error,
    // 其他错误码...
};

// 自定义错误类别
class errc_category : public boost::system::error_category {
public:
    const char* name() const noexcept override { return "rpc"; }
    
    std::string message(int ev) const override {
        switch (static_cast<errc>(ev)) {
            case errc::not_found: return "Resource not found";
            case errc::http_error: return "Http response error";
            default: return "Unknown error";
        }
    }
};


inline const boost::system::error_category& get_errc_category() {
    static errc_category instance;
    return instance;
}

// 允许隐式转换 errc -> error_code
inline boost::system::error_code make_error_code(errc e) {
    return {static_cast<int>(e), get_errc_category()};
}


// 自定义错误信息结构体
struct error_with_message {
  boost::system::error_code ec;
  std::string msg;

  // 添加隐式转换为 error_code 的能力
  operator boost::system::error_code() const noexcept {
      return ec;
  }
  // 添加获取错误消息的接口
  const std::string& message() const noexcept {
      return msg;
  }
};

// 带错误信息的 result 类型
template <class T>
using result_with_message = boost::system::result<T, error_with_message>;

// 标准 result 类型（不带额外信息）
template <class T>
using result = boost::system::result<T>;


} // namespace rpc


namespace boost {
namespace system {

template <typename T>
struct is_error_code_enum;
template <>
struct is_error_code_enum<rpc::errc> : std::true_type {
  static constexpr bool value = true;
};
} // namespace system
} // namespace boostamespace boost

#endif