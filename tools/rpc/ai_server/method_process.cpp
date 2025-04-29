#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/detail/error_code.hpp>
#include <openssl/err.h>
#include <openssl/tls1.h>
#include <ostream>
#include <string>
#include "log/logger_wrapper.h"
#include "method_process.h"
#include "result_with_message.h"


namespace chat{


MethodProcess::MethodProcess(boost::asio::io_context& iox) : iox_(iox), ssl_ctx_(boost::asio::ssl::context::sslv23)
{

}

MethodProcess::~MethodProcess()
{

}


void MethodProcess::Init(std::string remote_address)
{
 // ip::tcp::resolver::query query(remote_address, "https");
  // ip::tcp::resolver resolver(iox_);
  // endpoint_it_ = resolver.resolve(query);
  ssl_ctx_.set_default_verify_paths(); 
  ssl_ctx_.set_verify_mode(boost::asio::ssl::verify_peer);
  remote_address_ = remote_address;
}

/* 
  不与某个客户端建立长连接 .has_error / .value()
*/
boost::asio::awaitable<result_with_message<std::string>> MethodProcess::CallModelByHttps(std::string& user_query, std::string& api_key)
{
  std::string result; /* 这里要验证一下rvo */
  //std::string api_key_ = "example";
  
  boost::system::error_code ec;
  ip::tcp::resolver::query query(remote_address_, "https");
  ip::tcp::resolver resolver(iox_);
  ip::tcp::resolver::iterator endpoint_it_ = resolver.resolve(query);;  /* 连接端点 */
  /* 1. 建立TCP连接 */

  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_stream(iox_, ssl_ctx_);
  co_await boost::asio::async_connect(ssl_stream.next_layer(),  /* 获取底层TCP socket */
                                      endpoint_it_,
                                      boost::asio::redirect_error(boost::asio::use_awaitable,ec)
                                      );

  if (ec) {
    std::cout << "Error: " << ec.message() << std::endl;
    exit(-1);
  }

  /* 2. 设置SNI */          
  if (!SSL_set_tlsext_host_name(ssl_stream.native_handle(), remote_address_.c_str()) ) {
    LOG("Error") << "SSL_set_tlsext_host_name error";
    /* 获取错误码 */
    const auto openssl_err = ERR_get_error();
    boost::system::error_code ec(static_cast<int>(openssl_err),
                                 boost::asio::error::ssl_category
                                 );
    co_return error_with_message{ec, ec.message()};
  }

  /* 3. 执行SSL握手 客户端模式 */                                   
  co_await ssl_stream.async_handshake(boost::asio::ssl::stream_base::client, 
                                     boost::asio::redirect_error(boost::asio::use_awaitable,ec)
                                     );
  if (ec) {
    LOG("Error") << "SSL handshake failed: " << ec.message();
    co_return error_with_message{ec, ec.message()};
  }
  /* 
    4. 构建HTTP报文
    根据用户query和对应的apikey构建https报文
    后续这里应该就是要改成去redis查，构建一个随机码来存储对应的api userid sessin_id
  */
  boost::asio::streambuf stream_buf;
  BuildHttpsData(stream_buf,user_query, api_key);

  // 5. 发送数据（注意转换缓冲区类型）
  co_await boost::asio::async_write(ssl_stream,
                                    stream_buf,  // 自动转换为 ConstBufferSequence
                                 boost::asio::redirect_error(boost::asio::use_awaitable,ec)
                                   );
  if (ec) {
    LOG("Error") << "Http data async_write error: " << ec.message();
    co_return error_with_message{ec, ec.message()};
  }
  stream_buf.consume(stream_buf.size());

  // 6. 接收报文
  auto response_data = co_await RecvHttpsResponse(stream_buf, ssl_stream);

  if (response_data.has_error()){
    std::cout << "错误!" << std::endl;
    co_return error_with_message{response_data.error().ec, response_data.error().ec.message()};
  }

  std::cout << "接收报文为 : \n" << response_data.value() << std::endl;   
  // std::cout << static_cast<const void*>(response_data.value().data()) << std::endl;
  // co_return response_data.value();
  
  // 7. 处理报文
  auto respon_message = HttpProcessNotReason(response_data.value());
  
  std::cout << "回发消息为: " << respon_message << std::endl;

  co_return respon_message;
  // 8. 数据入库处理

  // 9. 回发信息

}

std::string MethodProcess::HttpProcessNotReason(std::string& HttpData){

  std::string rpc_respon_message;
    // 读取HTTP响应头的状态码
  std::function<std::string()> extract_HTTP_status = [&]() -> std::string {
      size_t first_line_end = HttpData.find("\r\n");
      if (first_line_end == std::string::npos)
          return "000";
      
      const std::string& status_line = HttpData.substr(0, first_line_end);
      
      // 直接查找状态码位置
      size_t first_space = status_line.find(' ');
      if (first_space == std::string::npos)
          return "000";
      
      size_t second_space = status_line.find(' ', first_space + 1);
      if (second_space == std::string::npos)
          return "000";
      
      return status_line.substr(first_space + 1, second_space - first_space - 1);
  };

  std::string status_code = extract_HTTP_status();
  if (status_code != "200") // HTTP 响应错误
  {
    LOG("Error") << "HTTP response error: " << status_code;
    //return "";
  }

    // HTTP 响应正确，处理HTTP_Body
  size_t header_end = HttpData.find("\r\n\r\n");
  std::string header = HttpData.substr(0, header_end + 4);
  std::transform(header.begin(), header.end(), header.begin(), ::tolower);

  std::string json_body = HttpData.substr(header_end + 4);

  // 检查是否是 Chunked 编码
  try
  {
    // 检查是否是 Chunked 编码
    bool is_chunked = header.find("transfer-encoding: chunked") != std::string::npos;
    if (is_chunked)
    {
        // Chunked 解码
        json_body = ParseChunkedBody(json_body);
    }
  } catch (const std::exception &e)
  {
      std::cerr << "Chunked 解码失败: " << e.what() << std::endl;
      return "";
  }

      // 解析json，提取内容
  try
  {

    /* {"error":{"message":"Incorrect API key provided. ","type":"invalid_request_error","param":null,"code":"invalid_api_key"},"request_id":"d4dea040-7e62-9e43-857f-a723792def05"}*/
      boost::json::value upstream_json = boost::json::parse(json_body);

      const auto& response_obj = upstream_json.as_object();
      if (response_obj.contains("error")) {
            const auto& error_obj = response_obj.at("error").as_object();
            rpc_respon_message = error_obj.at("message").as_string().c_str();
            // std::cerr << "API请求错误: " << error_msg << std::endl;
            return rpc_respon_message; // 返回空或携带错误信息
        }
      const auto &choices = upstream_json.at("choices").as_array();
      const auto &message = choices[0].at("message").as_object();

      rpc_respon_message = message.at("content").as_string().c_str();
     
  } catch (const std::exception &e)
  {
      std::cerr << "JSON 提取错误: " << e.what() << std::endl;
      return "";
  }

  return rpc_respon_message;
}

/* 
  从 Chunk块中解析出内容json
*/
std::string MethodProcess::ParseChunkedBody(const std::string& http_body){
  std::string rpc_response_body;
  std::stringstream ss(http_body);
  std::string line;

  // 解析 chunked 编码
  while (std::getline(ss, line))
  {
      // 解析 Chunk 大小
      size_t chunk_size = std::stoul(line, nullptr, 16); // 十六进制转换为十进制
      // std::cout << "Chunk 大小：" << chunk_size << std::endl;
      if (chunk_size == 0)
          break; // 结束块

      // 读取 Chunk 数据
      std::string chunk_data;
      chunk_data.resize(chunk_size);
      ss.read(&chunk_data[0], chunk_size);
      // std::cout << "Chunk 内容：" << chunk_data << std::endl;
      rpc_response_body += chunk_data;
      ss.ignore(2); // 去除可能的 CRLF
  }

  return rpc_response_body;
}

awaitable<result_with_message<std::string>> MethodProcess::RecvHttpsResponse(boost::asio::streambuf& response_stream, 
                                                                             boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& ssl_stream)
{
  boost::system::error_code ec;
  /* 准备返回的报文完整数据载体 */
  std::string response_data; //static_cast<const void*>(response_data.data())
  co_await boost::asio::async_read_until(ssl_stream, response_stream, "\r\n\r\n", boost::asio::use_awaitable);


  auto buffer_data = boost::asio::buffer_cast<const char*>(response_stream.data());
  std::string all_data(buffer_data, response_stream.size());
  size_t header_end = all_data.find("\r\n\r\n");
  std::cout << "header_end : " <<  header_end << std::endl;
  if (header_end == std::string::npos) {
      // 错误处理：未找到终止符
      co_return error_with_message{rpc::errc::not_found,"header end not found"};
  }
  std::string headers = all_data.substr(0, header_end + 4); 
  response_stream.consume(header_end + 4);

  /*******************************************************首部接收 *************************************************************/

  /* 这里应该插入一手判断的，判断是否为无消息体的场景 */
  /* 转换为小写用于字段检测 */
  std::string lower_headers(headers);
  std::transform(lower_headers.begin(), lower_headers.end(), lower_headers.begin(), ::tolower);
  /* 判断传输 */
  bool chunked = (lower_headers.find("transfer-encoding: chunked") != std::string::npos);
  size_t content_length = 0;

  /* 是否为chunk编码 */
  if (!chunked) {
    auto pos = lower_headers.find("content-length:");
    if (pos != std::string::npos){
      size_t colon = headers.find(':', pos);
      size_t endline = headers.find("\r\n", colon);
      std::string raw_value = headers.substr(colon+1, endline - colon - 1);
      /* 清理空白字符 */
      raw_value.erase(0, raw_value.find_first_not_of(" \t"));
      raw_value.erase(raw_value.find_last_not_of(" \t")+1);
      content_length = std::stoull(raw_value);
    }
  }

  /* 存储原始头部 */
  response_data = headers;

#ifdef DEBUG_INFO
  std::cout << "报文头部为 : \n" << response_data << std::endl;
#endif

  error_with_message result;
  if (chunked) {
    result = co_await Chunked_handler(response_data, response_stream, ssl_stream);
    //std::cout << "? " << std::endl;
  } else if ( content_length > 0) {
    result = co_await Content_handler(response_data, response_stream, ssl_stream, content_length);
  } else {
    result = co_await Eof_handler(response_data, response_stream, ssl_stream);
  }

  if (result.ec){
    LOG("Error") << "content data recv handler error";
    co_return result;
  }

  co_return std::move(response_data);
  
}

/* chunk编码 */
awaitable<error_with_message> MethodProcess::Chunked_handler(std::string& response_data,
                                                             boost::asio::streambuf& response_stream,
                                                             boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& ssl_stream
                                                             )
{
    boost::system::error_code ec;
    while(true)  
    {
      co_await boost::asio::async_read_until(ssl_stream, response_stream, "\r\n",boost::asio::use_awaitable);

      /* 分割 chunksize\r\n */
      {
        auto buffer_data = boost::asio::buffer_cast<const char*>(response_stream.data());
        std::string all_data(buffer_data, response_stream.size());
        size_t chunk_size_end = all_data.find("\r\n");
        if (chunk_size_end == std::string::npos) {
            // 错误处理：未找到终止符
             co_return error_with_message{rpc::errc::not_found,"chunk_size_end not found"};
        }
        size_t chunk_size  = std::stoul(all_data.substr(0, chunk_size_end) , nullptr, 16);
        response_data += all_data.substr(0, chunk_size_end + 2);
        response_stream.consume(chunk_size_end + 2);
        if (chunk_size == 0 ) co_return error_with_message{{},""};
      }
      
      /* 分割 chunkdata\r\n */
      co_await boost::asio::async_read_until(ssl_stream, response_stream, "\r\n",boost::asio::use_awaitable);

      {
        auto buffer_data = boost::asio::buffer_cast<const char*>(response_stream.data());
        std::string all_data(buffer_data, response_stream.size());
        size_t chunk_data_end = all_data.find("\r\n");
        response_data += all_data.substr(0, chunk_data_end + 2);
        response_stream.consume(chunk_data_end + 2);
      }

    }  
    // /* 处理终止块后的数据 */
    // co_await boost::asio::async_read_until(ssl_stream, response_stream, "\r\n\r\n", boost::asio::use_awaitable);
    // const auto& trailer_data = response_stream.data();
    // response_data.append(boost::asio::buffers_begin(trailer_data),
    //                      boost::asio::buffers_end(trailer_data));
    // response_stream.consume(trailer_data.size());
    co_return error_with_message{ec,""};
}         

/* 带有content */
awaitable<error_with_message> MethodProcess::Content_handler(std::string& response_data,
                                                             boost::asio::streambuf& response_stream,
                                                             boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& ssl_stream,
                                                             size_t content_length
                                                            )
{
    boost::system::error_code ec;
    size_t received = 0;
    while(received < content_length) {
      const size_t n = co_await boost::asio::async_read(ssl_stream, response_stream,
                                                        boost::asio::transfer_at_least(1),
                                                        boost::asio::use_awaitable);
      received += n;
      /* 追加数据 */
      const auto& data = response_stream.data();
      response_data.append(boost::asio::buffers_begin(data),
                           boost::asio::buffers_end(data));
      
      response_stream.consume(n);
    }
    co_return error_with_message{ec, ""};
}
/* 什么都没带 */                                                
awaitable<error_with_message> MethodProcess::Eof_handler(std::string& response_data,
                                                         boost::asio::streambuf& response_stream,
                                                         boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& ssl_stream
                                                         )
{
    boost::system::error_code ec;
    while (true) {
      const size_t n = co_await boost::asio::async_read(ssl_stream, response_stream,
                                                        boost::asio::transfer_at_least(1),
                                                        boost::asio::redirect_error(boost::asio::use_awaitable, ec));

      if (ec) {
        if (ec == boost::asio::error::eof) { ec.clear(); break;}
        else co_return error_with_message{ec, "eof_handler error: " + ec.message()};
      }

              // 追加数据到响应体
      const auto& data = response_stream.data();
      response_data.append(
          boost::asio::buffers_begin(data),
          boost::asio::buffers_end(data)
      );
      response_stream.consume(n);

    }

    co_return error_with_message{ec, ""};
}                                                                     

void MethodProcess::BuildHttpsData(boost::asio::streambuf& request, std::string& user_query, std::string& api_key)
{
  std::ostream request_stream(&request);
  std::string json_data = "{"
                                "\"model\":\"deepseek-r1-distill-qwen-1.5b\","
                                "\"messages\":[{"
                                "\"role\":\"user\","
                                "\"content\":\"" +
                                user_query + "\""
                                          "}]}";

  request_stream << "POST /compatible-mode/v1/chat/completions HTTP/1.1\r\n"
                 << "Host:" + remote_address_ + "\r\n"
                 << "Authorization: Bearer " + api_key + "\r\n"
                 << "Content-Type: application/json\r\n"
                 << "Content-Length: " + std::to_string(json_data.length()) + "\r\n"
                 << "Connection: keep-alive\r\n\r\n"
                 << json_data;                                          
}

} // namespace chat