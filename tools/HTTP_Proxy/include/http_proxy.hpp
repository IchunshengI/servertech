#ifndef INCLUDE_HTTP_REQUEST_CONTEXT_HPP
#define INCLUDE_HTTP_REQUEST_CONTEXT_HPP
#include <iostream>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

// Contains a request_context class, which encapsulates a Boost.Beast HTTP request
// and provides an easy way to build HTTP POST and GET.

namespace http_proxy
{
    namespace http = boost::beast::http;
    namespace ip = boost::asio::ip;

    class bridge : public boost::enable_shared_from_this<bridge>
    {
    public:
        typedef ip::tcp::socket socket_type;
        typedef boost::shared_ptr<bridge> ptr_type;

        bridge(boost::asio::io_context &ioc, std::string http)
            : downstream_socket_(ioc),
              upstream_socket_(ioc),
              http_host(http)
        {
        }
        socket_type &downstream_socket()
        {
            return downstream_socket_;
        }

        socket_type &upstream_socket()
        {
            return upstream_socket_;
        }

        void start(const ip::tcp::endpoint &endpoint);
        void handle_upstream_connect(const boost::system::error_code &error);

    private:
        void handle_downstream_write(const boost::system::error_code &error);
        void handle_downstream_read(const boost::system::error_code &error,
                                    const size_t &bytes_transferred);
        void handle_upstream_write(const boost::system::error_code &error);
        void handle_upstream_read(const boost::system::error_code &error,
                                  const size_t &bytes_transferred);

        void close();

        socket_type downstream_socket_;
        socket_type upstream_socket_;
        std::string http_host;
        enum
        {
            max_data_length = 8192
        }; // 8KB
        unsigned char downstream_data_[max_data_length];
        unsigned char upstream_data_[max_data_length];
        boost::asio::streambuf request_;

        boost::mutex mutex_;

    public:
        class acceptor
        {
        public:
            acceptor(boost::asio::io_context &io_context,
                     const std::string &local_host, unsigned short local_port,
                     const std::string &upstream_host)
                : io_context_(io_context),
                  localhost_address(boost::asio::ip::address_v4::from_string(local_host)),
                  acceptor_(io_context_, ip::tcp::endpoint(localhost_address, local_port)),
                  upstream_host_(upstream_host)
            {
            }

            bool accept_connections();

        private:
            void handle_accept(const boost::system::error_code &error);

            boost::asio::io_context &io_context_;
            ip::address_v4 localhost_address;
            ip::tcp::acceptor acceptor_;
            ptr_type session_;
            std::string upstream_host_;
        };
    };

} // namespace http_proxy

#endif
