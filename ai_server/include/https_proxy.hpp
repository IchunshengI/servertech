#ifndef INCLUDE_HTTPS_REQUEST_CONTEXT_HPP
#define INCLUDE_HTTPS_REQUEST_CONTEXT_HPP
#include <iostream>
#include <sstream>
#include <algorithm>
#include <arpa/inet.h>

#include <openssl/ssl.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/thread/mutex.hpp>

#include <boost/json.hpp>
#include <boost/algorithm/string.hpp>

// Contains a request_context class, which encapsulates a Boost.Beast HTTP request
// and provides an easy way to build HTTP POST and GET.

namespace https_proxy
{
    namespace ip = boost::asio::ip;
    namespace http = boost::beast::http;
    namespace ssl = boost::asio::ssl;
    namespace json = boost::json;

    class bridge : public boost::enable_shared_from_this<bridge>
    {
    public:
        typedef ip::tcp::socket socket_type;
        typedef ssl::stream<socket_type> ssl_socket_type;
        typedef boost::shared_ptr<bridge> ptr_type;

        bridge(boost::asio::io_context &ioc, ssl::context &ctx, std::string https)
            : downstream_socket_(ioc),
              upstream_socket_(ioc, ctx),
              https_host(https)
        {
        }

        socket_type &downstream_socket()
        {
            return downstream_socket_;
        }

        ssl_socket_type &upstream_socket()
        {
            return upstream_socket_;
        }

        void start(ip::tcp::resolver::iterator &endpoint_it);
        void handle_upstream_connect(const boost::system::error_code &error);
        void handle_upstream_SSL(const boost::system::error_code &error);

    private:
        void handle_downstream_write(const boost::system::error_code &error);
        void handle_downstream_read(const boost::system::error_code &error,
                                    const size_t &bytes_transferred);
        void handle_upstream_write(const boost::system::error_code &error);
        void handle_upstream_read(const boost::system::error_code &error,
                                  const size_t &bytes_transferred);

        void api_id_init();                                  // 初始化user_id, session_id, api_key, 未初始化则阻塞
        bool request_check(const size_t &bytes_transferred); // 转移报文至temp_downstream_data_，并检查是否完整, 完整则返回true, 否则返回false
        void create_request_(const std::string &content);    // 并创建请求报文

        bool http_accept_check(const size_t &bytes_transferred);      // 转移报文至temp_upstream_data_，并检查是否完整, 完整则返回true, 否则返回false
        bool is_chunked_body_complete(const std::string &response);   // 检查chunked编码的信息是否完整, 传入保存的temp_upstream_data_
        std::string parse_chunked_body(const std::string &http_body); // 读取并返回chunked编码后的信息

        void process_response_();                                                     // 处理HTTP响应报文和提取信息, 默认调用create_response_()存储信息
        void create_response_(const std::string &content);                            // 设置网络字节流信息, 只含回答
        void create_response_(const std::string &content, const std::string &reason); // 设置网络字节流信息, 含回答和推理

        void close();

        int mode = 1; // 0 为GET模式， 1为POST模式
        socket_type downstream_socket_;
        ssl_socket_type upstream_socket_;
        std::string https_host;

        boost::asio::streambuf downstream_data_;
        boost::asio::streambuf upstream_data_;
        boost::asio::streambuf request_;
        boost::asio::streambuf response_;

        std::string user_id;
        std::string session_id;
        std::string api_key;
        std::string temp_downstream_data_;
        std::string temp_upstream_data_;

        boost::mutex mutex_;

    public:
        class acceptor
        {
        public:
            acceptor(boost::asio::io_context &io_context, ssl::context &ssl_context,
                     const std::string &local_host, unsigned short local_port,
                     const std::string &upstream_host)
                : io_context_(io_context), ssl_context_(ssl_context),
                  localhost_address(boost::asio::ip::address_v4::from_string(local_host)),
                  acceptor_(io_context_, ip::tcp::endpoint(localhost_address, local_port)),
                  upstream_host_(upstream_host)
            {
            }

            bool accept_connections();

        private:
            void handle_accept(const boost::system::error_code &error);

            boost::asio::io_context &io_context_;
            ssl::context &ssl_context_;
            ip::address_v4 localhost_address;
            std::string upstream_host_;
            ip::tcp::acceptor acceptor_;
            ptr_type session_;
        };
    };

} // namespace https_proxy

#endif
