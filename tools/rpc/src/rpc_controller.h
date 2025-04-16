/*
	Date	  :	2025年4月14日
	Author	:	chunsheng
	Brief		:	rpc服务端控制器类，必须继承，因为目标类为抽象类，不能直接构造对象
*/

#ifndef RPC_CONTROLLER_H
#define RPC_CONTROLLER_H

#include <string>
#include "google/protobuf/service.h"

namespace rpc{
class RpcController : public google::protobuf::RpcController{
 public:
	RpcController() { Reset();}
	virtual ~RpcController() {}

	virtual void Reset() { is_failed_ = false; error_code_ = ""; }
	virtual bool Failed() const { return is_failed_; }
	virtual void SetFailed(const std::string& reason) { is_failed_ = true; error_code_ = reason; }
	virtual std::string ErrorText() const { return error_code_; }
	virtual void StartCancel() { }
	virtual bool IsCanceled() const { return false; }
	virtual void NotifyOnCancel(google::protobuf::Closure*) {};

 private:
	bool is_failed_;
	std::string error_code_;
};

} // namespace 

#endif

