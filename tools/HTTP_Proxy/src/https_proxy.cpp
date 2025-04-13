#include "https_proxy.hpp"

using namespace https_proxy;

void bridge::start(ip::tcp::resolver::iterator &endpoint_it)
{
    // TCP连接
    boost::asio::async_connect(
        upstream_socket_.next_layer(), endpoint_it,
        boost::bind(&bridge::handle_upstream_SSL,
                    shared_from_this(),
                    boost::asio::placeholders::error));
}

void bridge::handle_upstream_SSL(const boost::system::error_code &error)
{
    if (!error)
    {
        // 设置 SNI 主机名
        if (!SSL_set_tlsext_host_name(upstream_socket_.native_handle(), https_host.c_str()))
        {
            std::cerr << "SSL设置SNI主机错误: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
            close();
            return;
        }
        // SSL握手
        upstream_socket_.async_handshake(
            ssl::stream_base::client,
            boost::bind(&bridge::handle_upstream_connect,
                        shared_from_this(),
                        boost::asio::placeholders::error));
    }
    else
    {
        std::cerr << "TCP连接错误: " << error.message() << std::endl;
        close();
    }
}

void bridge::handle_upstream_connect(const boost::system::error_code &error)
{
    if (!error)
    {
        /* 读取远端云服务的响应报文，解析后转发处理结果给下游客户端 */
        async_read(
            upstream_socket_,
            upstream_data_, boost::asio::transfer_at_least(1),
            boost::bind(&bridge::handle_upstream_read,
                        shared_from_this(),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));

        /* 读取下游客户端的问题请求，并构建发送远端云服务的请求报文 */
        async_read(
            downstream_socket_,
            downstream_data_, boost::asio::transfer_at_least(1),
            boost::bind(&bridge::handle_downstream_read,
                        shared_from_this(),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        std::cerr << "SSL握手失败: " << error.message() << std::endl;
        close();
    }
}

void bridge::handle_downstream_write(const boost::system::error_code &error)
{
    if (!error)
    {
        async_read(
            upstream_socket_,
            upstream_data_, boost::asio::transfer_at_least(1),
            boost::bind(&bridge::handle_upstream_read,
                        shared_from_this(),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }
    else
        close();
}

void bridge::handle_downstream_read(const boost::system::error_code &error,
                                    const size_t &bytes_transferred)
{
    if (!error)
    {
        // 检查是否已经设置了 API-KEY
        if (bridge::api_key_set_check())
        {
            // 如果 API-KEY 已设置，继续处理请求
            bridge::create_request_();
            async_write(upstream_socket_,
                        request_,
                        boost::bind(&bridge::handle_upstream_write,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
        }
    }
    else
        close();
}

void bridge::handle_upstream_write(const boost::system::error_code &error)
{
    if (!error)
    {
        /* 这里会有分包和粘包？ */
        async_read(
            downstream_socket_,
            downstream_data_, boost::asio::transfer_at_least(1),
            boost::bind(&bridge::handle_downstream_read,
                        shared_from_this(),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }
    else
        close();
}

void bridge::handle_upstream_read(const boost::system::error_code &error,
                                  const size_t &bytes_transferred)
{
    if (error)
    {
        close();
        return;
    }

    if (bridge::http_accept_check())
    {
         std::cout << "?" << std::endl;
        // 报文完整，处理数据
        if (mode)
        {
            bridge::process_response_();
            async_write(downstream_socket_,
                        response_,
                        boost::bind(&bridge::handle_downstream_write,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
        }
        else
        {
            std::cout << "?" << std::endl;
            async_write(downstream_socket_,
                        upstream_data_,
                        boost::bind(&bridge::handle_downstream_write,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
        }
    }
}

void bridge::close()
{
    boost::mutex::scoped_lock lock(mutex_);

    if (downstream_socket_.is_open())
    {
        try
        {
            // 检查套接字是否连接并获取客户端 IP
            std::cout << "断开的客户端: " << this->downstream_socket().remote_endpoint().address().to_string() << std::endl;
        }
        catch (const boost::system::system_error &e)
        {
            // 捕获异常并输出错误信息
            std::cerr << "无法获取客户端 IP: " << e.what() << std::endl;
        }
        downstream_socket_.close();
    }

    if (upstream_socket_.next_layer().is_open())
    {
        upstream_socket_.next_layer().close();
    }

    // 清空缓冲区
    downstream_data_.consume(downstream_data_.size());
    upstream_data_.consume(upstream_data_.size());
    request_.consume(request_.size());
    response_.consume(response_.size());

    // 清楚API_KEY
    api_key.clear();
    temp_upstream_data_.clear();
}

bool bridge::acceptor::accept_connections()
{
    try
    {
        session_ = boost::shared_ptr<bridge>(new bridge(io_context_, ssl_context_, upstream_host_));

        acceptor_.async_accept(session_->downstream_socket(),
                               boost::bind(&acceptor::handle_accept,
                                           this,
                                           boost::asio::placeholders::error));
    }
    catch (std::exception &e)
    {
        std::cerr << "acceptor exception: " << e.what() << std::endl;
        return false;
    }

    return true;
}

void bridge::acceptor::handle_accept(const boost::system::error_code &error)
{
    if (!error)
    {
        // 获取客户端的ip
        auto remote_endpoint = session_->downstream_socket().remote_endpoint();
        std::cout << "连接的客户端: " << remote_endpoint.address().to_string()
                  << ":" << remote_endpoint.port() << std::endl;

        // 获取远端的域名
        ip::tcp::resolver::query query(upstream_host_, "https");
        std::cout << "远端的域名: " << query.host_name() << ", "
                  << "远端的服务: " << query.service_name() << std::endl;
        // 解释器
        ip::tcp::resolver resolver(io_context_);
        // 解析域名
        ip::tcp::resolver::iterator endpoint_it = resolver.resolve(query);

        session_->start(endpoint_it);

        if (!accept_connections())
        {
            std::cerr << "Failure during call to accept." << std::endl;
        }
    }
    else
    {
        std::cerr << "客户端连接失败: " << error.message() << std::endl;
    }
}

bool bridge::api_key_set_check()
{
    if (api_key.empty())
    {
        std::istream downstream(&downstream_data_);
        std::string content = std::string(std::istreambuf_iterator<char>(downstream), {});
        if (content.find("API-KEY:") == 0)
        {
            std::cout << "收到 API-KEY: " << content << std::endl;
            api_key = content.substr(8); // 提取 API-KEY
            std::cout << "API-KEY 已设置: " << api_key << std::endl;

            // 返回确认信息给客户端
            std::string response = "API-KEY 设置成功\n";
            bridge::create_response_(response);
            boost::asio::async_write(downstream_socket_,
                                     response_,
                                     boost::bind(&bridge::handle_upstream_write,
                                                 shared_from_this(),
                                                 boost::asio::placeholders::error));
        }
        else
        {
            // 提示用户输入 API-KEY
            std::string response = "error 0, 请先输入API-KEY:<your-key>\n";
            bridge::create_response_(response);
            boost::asio::async_write(downstream_socket_,
                                     response_,
                                     boost::bind(&bridge::handle_upstream_write,
                                                 shared_from_this(),
                                                 boost::asio::placeholders::error));
        }
        return false; // API-KEY 未设置或刚设置
    }

    return true; // API-KEY 已设置
}

bool bridge::http_accept_check()
{
    // 将新读取的数据追加到缓冲区
    temp_upstream_data_.append((std::istreambuf_iterator<char>(&upstream_data_)), {});
    // 检查是否包含完整的 HTTP 报文头部
    size_t header_end = temp_upstream_data_.find("\r\n\r\n");
    if (header_end == std::string::npos)
    {
        // 报文头部不完整，继续读取
        boost::asio::async_read(upstream_socket_, upstream_data_, boost::asio::transfer_at_least(1),
                                boost::bind(&bridge::handle_upstream_read,
                                            shared_from_this(),
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
        return false;
    }

    // 提取 HTTP 头部
    std::string header = temp_upstream_data_.substr(0, header_end + 4);

    // 检查是否是 Chunked 编码
    bool is_chunked = header.find("transfer-encoding: chunked") != std::string::npos;
    if (is_chunked)
    {
        // Chunked 编码，检查是否读取完整
        if (!is_chunked_body_complete(temp_upstream_data_))
        {
            // 报文不完整，继续读取
            boost::asio::async_read(upstream_socket_,
                                    upstream_data_,
                                    boost::asio::transfer_at_least(1),
                                    boost::bind(&bridge::handle_upstream_read,
                                                shared_from_this(),
                                                boost::asio::placeholders::error,
                                                boost::asio::placeholders::bytes_transferred));
            return false;
        }
    }
    else
    {
        // 非 Chunked 编码，检查 Content-Length
        size_t content_length_pos = header.find("content-length:");
        if (content_length_pos != std::string::npos)
        {
            size_t content_length_start = content_length_pos + 15;
            size_t content_length_end = header.find("\r\n", content_length_start);
            size_t content_length = std::stoul(header.substr(content_length_start, content_length_end - content_length_start));

            // 检查是否读取完整
            if (temp_upstream_data_.size() < header_end + 4 + content_length)
            {
                // 报文不完整，继续读取
                boost::asio::async_read(upstream_socket_,
                                        upstream_data_,
                                        boost::asio::transfer_at_least(1),
                                        boost::bind(&bridge::handle_upstream_read,
                                                    shared_from_this(),
                                                    boost::asio::placeholders::error,
                                                    boost::asio::placeholders::bytes_transferred));
                return false;
            }
        }
        else
        {
            // 非 Chunked 编码且没有 Content-Length, 清空数据，报错
            std::cerr << "HTTP 解析错误: 非 Chunked 编码且没有 Content-Length" << std::endl;
            std::string response_body = "error: 1, HTTP 解析错误";
            bridge::create_response_(response_body);
            async_write(downstream_socket_,
                        response_,
                        boost::bind(&bridge::handle_downstream_write,
                                    shared_from_this(),
                                    boost::asio::placeholders::error));
            return false;
        }
    }
    // 报文完整，返回 true
    return true;
}

void bridge::process_response_()
{
    std::string response_body;
    std::string response_reason;
    std::cout << temp_upstream_data_ << std::endl;

    // 读取HTTP响应头的状态码
    std::function<std::string()> extract_HTTP_status = [&]()
    {
        size_t first_line_end = temp_upstream_data_.find("\r\n");
        if (first_line_end == std::string::npos)
            return std::string("000");
        std::string status_line = temp_upstream_data_.substr(0, first_line_end);
        std::vector<std::string> status_parts;
        boost::split(status_parts, status_line, boost::is_any_of(" "));

        return status_parts[1];
    };
    std::string status_code = extract_HTTP_status();
    if (status_code != "200") // HTTP 响应错误
    {
        std::cerr << "HTTP 响应错误: " << status_code << std::endl;
        response_body = "error: 2, HTTP 响应错误";
        bridge::create_response_(response_body);
        temp_upstream_data_.clear();
        return;
    }

    // HTTP 响应正确，处理HTTP_Body
    size_t header_end = temp_upstream_data_.find("\r\n\r\n");
    std::string json_body = temp_upstream_data_.substr(header_end + 4);
    try
    {
        // 检查是否是 Chunked 编码
        bool is_chunked = temp_upstream_data_.find("transfer-encoding: chunked") != std::string::npos;
        if (is_chunked)
        {
            // Chunked 解码
            json_body = parse_chunked_body(json_body);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Chunked 解码失败: " << e.what() << std::endl;
        response_body = "error: 3, Chunked 解码失败";
        bridge::create_response_(response_body);
        temp_upstream_data_.clear();
        return;
    }

    // std::cout << json_body << std::endl;
    // 解析json，提取内容
    try
    {
        json::value upstream_json = json::parse(json_body);
        // json::object res_json;
        const auto &choices = upstream_json.at("choices").as_array();
        const auto &message = choices[0].at("message").as_object();
        // const auto &usage = upstream_json.at("usage").as_object();

        // res_json["content"] = message.at("content").as_string().c_str();
        // res_json["reasoning_content"] = message.at("reasoning_content").as_string().c_str();
        // res_json["finish_reason"] = choices[0].at("finish_reason").as_string().c_str();
        // res_json["usage"] = usage.at("total_tokens");
        // res_json["model"] = upstream_json.at("model").as_string().c_str();
        // res_json["created"] = upstream_json.at("created");

        // // 序列化为字符串
        // response_body = json::serialize(res_json);

        response_body = message.at("content").as_string().c_str();
        response_reason = message.at("reasoning_content").as_string().c_str();
    }
    catch (const std::exception &e)
    {
        std::cerr << "JSON 提取错误: " << e.what() << std::endl;
        response_body = "error: 4, JSON 提取错误";
        bridge::create_response_(response_body);
        return;
    }
    // 导入response_
    // std::cout << "回答: " << response_body << std::endl;
    // std::cout << "推理: " << response_reason << std::endl;
    bridge::create_response_(response_body, response_reason);
}

void bridge::create_response_(const std::string &content, const std::string &reason)
{
    std::ostream response_stream(&response_);
    // 使用write避免整数被转换成ASCII字符串,明确写入4字节长度
    // 处理总长度
    uint32_t total_len = htonl(content.length() + reason.length() + 12);
    response_stream.write(
        reinterpret_cast<const char *>(&total_len),
        sizeof(total_len));

    // 处理内容
    uint32_t content_len = htonl(content.length());
    response_stream.write(
        reinterpret_cast<const char *>(&content_len),
        sizeof(content_len));

    response_stream.write(
        content.data(),
        content.size());

    // 处理推理
    uint32_t reason_len = htonl(reason.length());
    response_stream.write(
        reinterpret_cast<const char *>(&reason_len),
        sizeof(reason_len));

    response_stream.write(
        reason.data(),
        reason.size());

    temp_upstream_data_.clear();
}

/*
    响应给客户端的内容，格式为
    size(4) + data
*/
void bridge::create_response_(const std::string &response_message)
{
    std::ostream response_stream(&response_);
    // 使用write避免整数被转换成ASCII字符串,明确写入4字节长度
    // 处理总长度
    //uint32_t total_len = htonl(response_message.length() + 8);
    // response_stream.write(
    //     reinterpret_cast<const char *>(&total_len),
    //     sizeof(total_len));

    // 处理内容
    uint32_t message_len = htonl(response_message.length());
    response_stream.write(
        reinterpret_cast<const char *>(&message_len),
        sizeof(message_len));
    std::cout << "发送客户端的数据大小为 " << response_message.length() <<std::endl;
    response_stream.write(
        response_message.data(),
        response_message.size());
    std::cout << "发送客户端的总大小为 " << response_message.size()+ 4 <<std::endl;
    temp_upstream_data_.clear();
}

void bridge::create_request_()
{
    std::ostream request_stream(&request_);
    std::istream downstream(&downstream_data_);
    std::string content = std::string(std::istreambuf_iterator<char>(downstream), {});
    // 获取客户端的ip
    auto remote_endpoint = this->downstream_socket().remote_endpoint();

    if (mode)
    {
        // POST 请求
        std::string json_data = "{"
                                "\"model\":\"deepseek-r1-distill-qwen-1.5b\","
                                "\"messages\":[{"
                                "\"role\":\"user\","
                                "\"content\":\"" +
                                content + "\""
                                          "}]}";
        request_stream << "POST /compatible-mode/v1/chat/completions HTTP/1.1\r\n"
                       << "Host:" + https_host + "\r\n"
                       << "Authorization: Bearer " + api_key + "\r\n"
                       << "Content-Type: application/json\r\n"
                       << "Content-Length: " << json_data.length() << "\r\n"
                       << "Connection: keep-alive\r\n\r\n"
                       << json_data;

        std::cout << remote_endpoint.address().to_string()
                  << " POST request: " << content << std::endl;
    }
    else
    {
        // GET 请求
        request_stream << "GET /" << content << " HTTP/1.1\r\n";
        request_stream << "Host: " << https_host << "\r\n";
        request_stream << "Accept: */* \r\n";
        request_stream << "Connection: keep-alive\r\n\r\n";
        std::cout << remote_endpoint.address().to_string()
                  << " GET request: " << https_host + '/' + content << std::endl;
    }
}

bool bridge::is_chunked_body_complete(const std::string &response)
{
    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return false;

    std::string body = response.substr(header_end + 4);
    std::stringstream ss(body);
    std::string line;

    while (std::getline(ss, line))
    {
        // 解析 Chunk 大小
        size_t chunk_size = std::stoul(line, nullptr, 16); // 十六进制转换为十进制

        if (chunk_size == 0)
            return true; // 结束块

        // 跳过 Chunk 编码的数据和 CRLF
        ss.ignore(chunk_size + 2);
    }

    return false;
}

std::string bridge::parse_chunked_body(const std::string &http_body)
{
    std::stringstream ss(http_body);
    std::string decoded_body;
    std::string line;

    // 解析 chunked 编码
    while (std::getline(ss, line))
    {
        // 解析 Chunk 大小
        size_t chunk_size = std::stoul(line, nullptr, 16); // 十六进制转换为十进制

        if (chunk_size == 0)
            break; // 结束块

        // 读取 Chunk 数据
        std::getline(ss, line);
        decoded_body += line.substr(0, chunk_size); // 去除可能的 CRLF
    }

    return decoded_body;
}