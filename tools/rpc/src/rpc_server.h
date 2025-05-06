/*
	Date	  :	2025年4月14日
	Authore : chunsheng
	Brief		: RPC服务端基类头文件
*/

#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <string>
#include "google/protobuf/service.h"
#include "boost/asio.hpp"
#include <boost/asio/spawn.hpp> 
#include "boost/asio/awaitable.hpp"
#include <boost/asio/co_spawn.hpp>
#include "result_with_message.h"
namespace rpc{

/* 
    RPC服务基类，监听 + 注册 + 调用 + 响应回调
*/  
class RpcServer{
 public:
  virtual ~RpcServer() {}

  virtual boost::asio::awaitable<error_with_message> Start(std::string& server_addr) = 0;             /* 开始监听 + 注册 */
  virtual bool RegisterService(google::protobuf::Service* service) = 0;   														/* 注册提供的服务方法 */
  virtual void ProcRpcData(const std::string& service_id, 																					  /* 远程调用请求执行, 这里是纯业务，其实是可以交由线程池来处理的 */	
													 const std::string& method_id,
                           const std::string& serialzied_data,
                           const std::shared_ptr<boost::asio::ip::tcp::socket>& socket
                           ) = 0;
  virtual boost::asio::awaitable<void> CallbackDone(google::protobuf::Message* response_msg,																	/* 执行完成之后的回调,使用协程 */
                            												std::shared_ptr<boost::asio::ip::tcp::socket> socket
                           													) = 0;
};

/* 这里后期要加上去zookeeper去注册的的操作 */
std::unique_ptr<RpcServer> create_rpc_server(boost::asio::any_io_executor ex);

} // namespace rpc


#endif