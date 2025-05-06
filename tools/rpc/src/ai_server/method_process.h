/*
  Date	  :	2025年4月23日
  Author	:	Weiktang & chunsheng
  Brief		:	rpc远端服务方法处理类
*/
#include <boost/asio/io_context.hpp>
#include <boost/asio/streambuf.hpp>
#include "result_with_message.h"
#pragma 
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include "result_with_message.h"
namespace chat{

namespace ip = boost::asio::ip;
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;

using boost::asio::awaitable;
using rpc::result_with_message;
using rpc::error_with_message;
/* 服务方法的具体操作类 */
class MethodProcess{
 public:
  MethodProcess(boost::asio::any_io_executor);
  ~MethodProcess();

  /* 这里应该是一个协程函数,返回结果的话应该是解析后的字符串,这里到时候测试一下RVO */
  awaitable<result_with_message<std::string>> CallModelByHttps(std::string& user_query, std::string& api_key);
  void Init(std::string remote_address);     /* 初始化操作 */
 private:
  /* 构建http报文数据 */
  void BuildHttpsData(boost::asio::streambuf&,std::string& user_query, std::string& api_key);  
  /* 接收云端http报文数据 */
  awaitable<result_with_message<std::string>> RecvHttpsResponse(boost::asio::streambuf&,
                                                                boost::asio::ssl::stream<boost::asio::ip::tcp::socket>&
                                                                );
  /* 处理云端http报文数据 */
  std::string HttpProcessNotReason(std::string&);
  /* 解析chunked数据 */
  std::string ParseChunkedBody(const std::string& http_body);
  /* 报文首部编码方式对应的接收手段 */
  /* chunk编码 */
  awaitable<error_with_message> Chunked_handler(std::string&,
                                                boost::asio::streambuf&,
                                                boost::asio::ssl::stream<boost::asio::ip::tcp::socket>&
                                                );
  /* 带有content */
  awaitable<error_with_message> Content_handler(std::string&,
                                                boost::asio::streambuf&,
                                                boost::asio::ssl::stream<boost::asio::ip::tcp::socket>&,
                                                size_t
                                                );
  /* 什么都没带 */                                                
  awaitable<error_with_message> Eof_handler(std::string&,
                                            boost::asio::streambuf&,
                                            boost::asio::ssl::stream<boost::asio::ip::tcp::socket>&
                                            );                                                                                                
  boost::asio::any_io_executor ex_;
  boost::asio::ssl::context ssl_ctx_;        /* ssl 上下文*/
  std::string remote_address_;               /* 服务端地址 */
}; // namespace chat
}

