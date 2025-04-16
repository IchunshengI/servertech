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
// 自定义错误信息结构体
struct error_with_message {
    boost::system::error_code ec;
    std::string msg;
};

// 带错误信息的 result 类型
template <class T>
using result_with_message = boost::system::result<T, error_with_message>;

// 标准 result 类型（不带额外信息）
template <class T>
using result = boost::system::result<T>;

} // namespace rpc

#endif