/*
	Date	  :	2025年4月14日
	Author	: chunsheng	
	Brief		: rpc服务端实现类，启用c++20协程功能
*/

#include "config/config.h"
#include "rpc_server.h"
#include "log/logger_wrapper.h"
#include "rpc_controller.h"
#include "rpc_meta.pb.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include "zookeeper_op.h"
using namespace rpc;
namespace rpc
{

using chat::LOG;

class RpcServerImpl final : public RpcServer
{
 public:
  RpcServerImpl(boost::asio::io_context& iox) : iox_(iox)
  {
		chat::InitLogger(iox_);
  }

  ~RpcServerImpl() 
    final override
  {

  }


/* 
	创建服务端点, 准备接收远程请求
*/
  boost::asio::awaitable<error_with_message> Start(std::string& server_addr) 
    final override
  {

		size_t split_pos = server_addr.find(':');
		std::string ip = server_addr.substr(0, split_pos);
		std::string port = server_addr.substr(split_pos + 1);   
		if(ip.empty() || port.empty()){
			
			LOG("Error") << "Miss Ip:port info";
			co_return error_with_message{
				boost::system::errc::make_error_code(boost::system::errc::invalid_argument),
				"Miss Ip:port info"
			};
		}

		/* 创建端点 */
		boost::system::error_code ec;
		auto address = boost::asio::ip::make_address(ip, ec);
		if (ec) {
			LOG("Error") << "Invalid Ip address";
			co_return error_with_message{ec, "Invalid Ip address"};
		}
		boost::asio::ip::tcp::endpoint ep(address, std::stoi(port));
		boost::asio::ip::tcp::acceptor acceptor(co_await boost::asio::this_coro::executor, ep);
    

    std::string rpcIp = chat::Config::Instance().Load("rpcserverIp");
    std::string rpcPort = (chat::Config::Instance().Load("rpcserverPort"));

    /* 注册zookeeper服务 */
    ZkClient zkCli;
    zkCli.Start();
    /* 加入zookeeper服务器 */
    for (auto &sp : services_) 
    {
        // /service_name   /UserServiceRpc
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
         // 每方法服务实例创建 ***************************************************************************
        // for (auto &mp : sp.second.mdescriptor_)
        // {   
        //   // /service_name/method_name   /UserServiceRpc/Login 存储当前这个rpc服务节点主机的ip和port
        //   std::string method_path = service_path + "/" + mp.first;
        //   zkCli.Create(method_path.c_str(), nullptr, 0,0); /* 持久化服务下的方法*/
        //   // 3. 在方法节点下创建临时顺序子节点（实例节点）
        //   std::string instance_path = method_path + "/instance_";
        //   char method_path_data[128] = {0};
        //   sprintf(method_path_data, "%s:%d", rpcIp.c_str(), rpcPort);
        //   // ZOO_EPHEMERAL表示znode是一个临时性节点
        //   zkCli.Create(instance_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL|ZOO_SEQUENCE);        
        // }
        // 每方法服务实例创建 ***************************************************************************

        // 每服务实例创建 ***************************************************************************
        std::string instance_path = service_path + "/instance_";
        //char method_path_data[128] = {0};
        //sprintf(method_path_data, "%s:%d", rpcIp.c_str(), rpcPort);
        std::string service_path_data = rpcIp + ":" + rpcPort;
        // ZOO_EPHEMERAL表示znode是一个临时性节点
        zkCli.Create(instance_path.c_str(), service_path_data.c_str(), service_path_data.size(), ZOO_EPHEMERAL|ZOO_SEQUENCE);
        // 每服务实例创建 ***************************************************************************

    }

		/* 存储一下服务端点信息 */
		server_addr_ = server_addr;		
		/* 接收连接 */
		while(true){
			auto socket = std::make_shared<boost::asio::ip::tcp::socket>(co_await boost::asio::this_coro::executor);
			co_await acceptor.async_accept(*socket, boost::asio::redirect_error(boost::asio::use_awaitable,ec));
			if (ec) {
				LOG("Error") << "Failed to accepr connection";
				co_return error_with_message{ec, "Failed to accepr connection"};
			}

			/* 为每个连接启动独立协程 */
			co_spawn(
				acceptor.get_executor(),[this, socket]{
					return HandleConnection(std::move(socket));
				}, boost::asio::detached
			);
		}
		co_return error_with_message{};
  }

  bool RegisterService(google::protobuf::Service* service)
    final override
  {
    std::string    method_id;
    ServiceInfo    service_info;
    const google::protobuf::ServiceDescriptor* sdescriptor = service->GetDescriptor();
    for (int i=0; i< sdescriptor->method_count(); ++i){
      method_id = sdescriptor->method(i)->name();
      service_info.mdescriptor_[method_id] = sdescriptor->method(i);
    }

    service_info.service_ = service;
    //services_[sdescriptor->name()] = service_info;
    services_.emplace(sdescriptor->name(), std::move(service_info));
	  return true;
  }

  void ProcRpcData(const std::string& service_id,
									 const std::string& method_id,
                   const std::string& serialzied_data,
                   const std::shared_ptr<boost::asio::ip::tcp::socket>& socket) 
    final override
  {
		auto service = services_[service_id].service_;
		auto mdescriptor = services_[service_id].mdescriptor_[method_id];
		auto recv_msg = service->GetRequestPrototype(mdescriptor).New();
		auto resp_msg = service->GetResponsePrototype(mdescriptor).New();

		recv_msg->ParseFromString(serialzied_data);

		/* 回调 */
    // 使用自定义回调类包装，因为callmethod中不支持lambda，所以咱得包装一层
    struct LambdaCallback : public google::protobuf::Closure {
        std::function<void()> func;
        void Run() override { func(); }
        ~LambdaCallback(){
#ifdef DEBUG_INFO          
          LOG("Debug") << "~LambdaCallback";
#endif          
        }
    };
    
    auto* callback = new LambdaCallback;
   // auto callback = std::make_shared<LambdaCallback>();
    // std::weak_ptr<LambdaCallback> weak_cb = callback;
    callback->func = [this, resp_msg, socket,callback](){
        //auto self = callback;
        boost::asio::co_spawn(
            socket->get_executor(),
            [this, resp_msg, socket,callback]() -> boost::asio::awaitable<void> {
                co_await CallbackDone(resp_msg, socket);
                delete callback;
            },
            boost::asio::detached
        );
    };																

		RpcController* controller_ptr = new RpcController();
		service->CallMethod(mdescriptor, controller_ptr, recv_msg, resp_msg, callback);
  }

  boost::asio::awaitable<void> CallbackDone(google::protobuf::Message* response_msg,
                    std::shared_ptr<boost::asio::ip::tcp::socket> socket)
    final override
  {
    //sleep(2);
		int serialized_size = response_msg->ByteSizeLong();
		std::string resp_data;
#ifdef DEBUG_INFO   
    LOG("Debug") << "serialized_size :" <<  serialized_size;
#endif
		resp_data.resize(sizeof(uint32_t) + serialized_size);

		/* 按网络字节序写入长度头 */
		*reinterpret_cast<uint32_t*>(resp_data.data()) = htonl(serialized_size);

		if(!response_msg->SerializeToArray(resp_data.data() + sizeof(uint32_t), serialized_size)){
      LOG("Error") << "Failed to serialize response message";
      co_return;
    }

		/* 协程发送 */
		boost::system::error_code ec;
		co_await boost::asio::async_write( *socket,
																			 boost::asio::buffer(resp_data),
																			 boost::asio::redirect_error(boost::asio::use_awaitable, ec)
																			 );
		if (ec) {
			LOG("Error") << "resp_data async_write error: " << ec.message();
		}
#ifdef DEBUG_INFO      
    LOG("Debug") << "Success send :" <<  serialized_size;
#endif
    delete response_msg;
  }

 private:
	/* 
		处理单个连接
	*/
	boost::asio::awaitable<void> HandleConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket){
		boost::system::error_code ec;

    while(true){
		/* 接收元数据大小 */
		uint32_t rpc_meta_size;
		//char rpc_meta_buf[4];
		co_await socket->async_receive(boost::asio::buffer((char*)(&rpc_meta_size), sizeof(rpc_meta_size)), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
      if (ec != boost::asio::error::eof){
        LOG("Error") << "socket rpc_meta_size async_receive error" << ec.message();
      }
			co_return;
		}
		rpc_meta_size = ntohl(rpc_meta_size);  // 网络字节序 → 主机字节序
#ifdef DEBUG_INFO      
    LOG("Debug") << "rpc_meta_size : " << rpc_meta_size;
#endif
		/* 接收元数据 */
		std::vector<char> rpc_meta_data(rpc_meta_size);
		co_await socket->async_receive(boost::asio::buffer(rpc_meta_data), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			LOG("Error") << "socket rpc_meta_data async_receive error";
			co_return;
		}

		/* 解析数据 rpc_meta_size + rpc_meta */
		RpcMeta rpc_meta_data_proto;
		if (!rpc_meta_data_proto.ParseFromArray(rpc_meta_data.data(), rpc_meta_data.size())){
			LOG("Error") << "rpc_meta_data_proto parseFromArray error";
			co_return;
		}

		/* 接收请求数据 */
		std::vector<char> request_data(rpc_meta_data_proto.data_size());
		co_await socket->async_receive(boost::asio::buffer(request_data), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if (ec) {
			LOG("Error") << "request_data async_receive error";
			co_return;
		}
#ifdef DEBUG_INFO  
		LOG("Debug") << "Server recv info:\n" 
								 << "service_id: " << rpc_meta_data_proto.service_id() << "\n"
								 << "method_id: " << rpc_meta_data_proto.method_id() << "\n"
								 << "data_size: " << rpc_meta_data_proto.data_size();
#endif
		/* rpc调用 */
		ProcRpcData(rpc_meta_data_proto.service_id(),
							  rpc_meta_data_proto.method_id(),
								std::string(request_data.data(),request_data.size()),
								socket);
    }

	}

 	boost::asio::io_context& iox_; /* 协程上下文 */
  std::string server_addr_;
  struct ServiceInfo { /* 单个服务的信息单元 */
    google::protobuf::Service* service_;
    std::map<std::string, const google::protobuf::MethodDescriptor*> mdescriptor_;
  };
  std::map<std::string, ServiceInfo> services_; /* 映射多个服务 */
};

std::unique_ptr<RpcServer> create_rpc_server(boost::asio::io_context& iox){

  return std::make_unique<RpcServerImpl>(iox);
}

} // namespace rpc


