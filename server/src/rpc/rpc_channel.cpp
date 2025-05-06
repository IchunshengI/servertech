/*
  Date	  :	2025年4月15日
  Author	:	chunsheng
  Brief		:	客户端调用channel的具体实现类
*/
#include "rpc/rpc_channel.h"
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <coroutine> 
#include "rpc/result_with_message.h"
#include "log/logger_wrapper.h"
#include "google/protobuf/message.h"
#include "rpc/rpc_meta.pb.h"
#include "rpc/zookeeper_op.h"
namespace rpc{

//using chat::LOG;
class RpcChannelImp : public RpcChannel{
 public:
  RpcChannelImp(std::string server_name, boost::asio::any_io_executor ex) : server_name_(server_name), ex_(ex), socket_(ex) {
  }
  ~RpcChannelImp () override{
#ifdef DEBUG_INFO  
    LOG("Debug") << "~RpcChannelImp";
#endif    
  }
  void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller,
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          google::protobuf::Closure* done
                          )
      final override
   {
      /* 发送格式: meta_size + meta_data + request_data */
      std::string request_data_str;
      if (!request->SerializeToString(&request_data_str)){
        std::cout<< "serialize request error" << std::endl;
        controller->SetFailed("serialize request error");
        return;
      }
      /* rpc元数据构建 */
      RpcMeta rpc_meta;
      rpc_meta.set_service_id(method->service()->name());
      rpc_meta.set_method_id(method->name());
      rpc_meta.set_data_size(request_data_str.size());
      std::string rpc_meta_str;
      if (!rpc_meta.SerializeToString(&rpc_meta_str)){
        std::cout<< "serialize rpc_meta error" << std::endl;
        controller->SetFailed("serialize rpc_meta error");
        return;
      }
      
      std::string serialzied_str;
#ifdef DEBUG_INFO        
      LOG("Debug") << "rpc_meta_size : " << rpc_meta_str.size();
      LOG("Debug") << "request_size : " << request_data_str.size();
#endif
      /* 网络字节序写入 */
      uint32_t rpc_meta_size = htonl(rpc_meta_str.size());
      serialzied_str.append(reinterpret_cast<const char*>(&rpc_meta_size), sizeof(uint32_t));
      serialzied_str += rpc_meta_str;
      serialzied_str += request_data_str;     

      boost::asio::co_spawn(ex_,
                            [this, send_data= std::move(serialzied_str), response,done] () -> boost::asio::awaitable<void> {
                               /* 挂起点,等待 */
                               auto ec = co_await Handle_write_recv(send_data,response); 
                               if (ec) {
                                 LOG("Error") << "Handle_write_recv error" << ec.message();
                               }

                               /* 执行回调，恢复主协程 */
                               if(done) done->Run(); 
                               co_return;
                            },
                            boost::asio::detached);
   }
 private:
  /* 
    静态回调函数
  */
  static void zk_watcher(zhandle_t *zh  __attribute__((unused)), 
                         int type,
                         int state, const char* path __attribute__((unused)), 
                         void *watcherCtx __attribute__((unused))
                         )
  {
    if (type == ZOO_SESSION_EVENT){
    /* 初始化连接状态 */
    if (state == ZOO_EXPIRED_SESSION_STATE){
      // RpcChannelImp * rpc_channel_t = (RpcChannelImp*)zoo_get_context(zh);
      // rpc_channel_t->Start();
      /* 简单做法吧，直接客户端错误 */
      LOG("Error") << "can not connect to the server!";
      exit(-1);
    } 
  }
  }

  /* 
    初始化 
  */
  boost::asio::awaitable<error_with_message> Start() override{
    boost::system::error_code ec;
    if (socket_.is_open()){
      socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both); 
      socket_.close(ec);
      if (ec){
        LOG("Error") << "socket close error!";
        co_return error_with_message{ec, "socket close error!"};
      }
    }

    /* 
      这里重新加载一次zookeeper的服务端点
    */
    ZkClient cli;
    cli.Start();
    std::string server_addr_ = cli.GetData(("/"+server_name_).c_str(), zk_watcher, this);
    //std::string server_addr_ = "192.168.1.100:8000";

    size_t split_pos = server_addr_.find(":");
    std::string ip = server_addr_.substr(0, split_pos);
    std::string port = server_addr_.substr(split_pos + 1,4);
    //std::cerr << "port:" << port << std::endl;
    //LOG("Error") << server_addr_;
    //exit(-1);    
    if(ip.empty() || port.empty()){
			
			LOG("Error") << "Miss Ip:port info";
			co_return error_with_message{
				boost::system::errc::make_error_code(boost::system::errc::invalid_argument),
				"Miss Ip:port info"
			};
		}


    /* 解析端点 */
    //boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver(ex_);
    auto endpoints = co_await resolver.async_resolve(ip, 
                                                                                  port,
                                                                                  boost::asio::redirect_error(boost::asio::use_awaitable,ec)
                                                                                  );
    if (ec) {
      LOG("Error") << "resolver async_resolve error" << ec.message() << std::endl << "ip :" << ip << " port: " << stoi(port); 
      co_return error_with_message{
        ec, " resolver async_resolve error"
      };
    }

    co_await boost::asio::async_connect(socket_,
                                        endpoints,
                                        boost::asio::redirect_error(boost::asio::use_awaitable,ec)
                                        );
    if (ec) {
      LOG("Error") << "socket async_connect error : " << ec.message();
      co_return error_with_message{
        ec, "socket async_connect error"
      };
    }
    co_return error_with_message{};
  }

  /* 读取并接收 */
  boost::asio::awaitable<boost::system::error_code> Handle_write_recv(const std::string& send_data, google::protobuf::Message* response){
    boost::system::error_code ec;
    /* 发送数据 */
    co_await boost::asio::async_write(socket_,
                                      boost::asio::buffer(send_data),
                                      boost::asio::redirect_error(boost::asio::use_awaitable, ec)
                                      );
    if (ec) {
      LOG("Error") << "Handle_write_recv async_write error " << ec.message();
      co_return ec;
    }

    /* 接收响应长度 */
    uint32_t resp_len;
    co_await socket_.async_receive(boost::asio::buffer(reinterpret_cast<char*>(&resp_len), sizeof(resp_len)),
                                    boost::asio::redirect_error(boost::asio::use_awaitable,ec)
                                    );
    if (ec) {
      LOG("Error") << "Handle_write_recv async_receive resp_len error" << ec.message();
      co_return ec;
    }
    resp_len = ntohl(resp_len);
    if (resp_len == 0){
      LOG("Error") << "Handle_write_recv resp_len error";
      co_return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }
    std::vector<char> resp_data(resp_len);
    co_await socket_.async_receive(boost::asio::buffer(resp_data),
                                   boost::asio::redirect_error(boost::asio::use_awaitable, ec)
                                   );
    if (ec) {
      co_return ec;
    }
    
    /* 反序列化 */
    if (!response->ParseFromArray(resp_data.data(),resp_data.size())){
      LOG("Error") << "Handle_write_recv ParseFromArray error";
      co_return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }
    // std::cerr << "全部接收完成" << std::endl;
    co_return boost::system::error_code{};
  }

  std::string server_name_;
  boost::asio::any_io_executor ex_;
  boost::asio::ip::tcp::socket socket_;

};

std::shared_ptr<RpcChannel> create_rpc_channel(std::string server_name, boost::asio::any_io_executor ex){
  return std::make_shared<RpcChannelImp>(std::move(server_name), ex);
}
} // namespace rpc
