#include "http_proxy.hpp"

using namespace http_proxy;

void bridge::start(const ip::tcp::endpoint &endpoint)
{
    upstream_socket_.async_connect(
        endpoint,
        boost::bind(&bridge::handle_upstream_connect,
                    shared_from_this(),
                    boost::asio::placeholders::error));
}

void bridge::handle_upstream_connect(const boost::system::error_code &error)
{
    if (!error)
    {
        upstream_socket_.async_read_some(
            boost::asio::buffer(upstream_data_, max_data_length),
            boost::bind(&bridge::handle_upstream_read,
                        shared_from_this(),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));

        downstream_socket_.async_read_some(
            boost::asio::buffer(downstream_data_, max_data_length),
            boost::bind(&bridge::handle_downstream_read,
                        shared_from_this(),
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }
    else
        close();
}

void bridge::handle_downstream_write(const boost::system::error_code &error)
{
    if (!error)
    {
        upstream_socket_.async_read_some(
            boost::asio::buffer(upstream_data_, max_data_length),
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
        std::string content(reinterpret_cast<const char *>(downstream_data_), bytes_transferred);
        std::ostream request_stream(&request_);
        request_stream << "GET " << '/' + content << " HTTP/1.1\r\n";
        request_stream << "Host: " << http_host << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: keep-alive\r\n\r\n";

        std::cout << "request: " << http_host + '/' + content << std::endl;

        async_write(upstream_socket_,
                    request_,
                    boost::bind(&bridge::handle_upstream_write,
                                shared_from_this(),
                                boost::asio::placeholders::error));
    }
    else
        close();
}

void bridge::handle_upstream_write(const boost::system::error_code &error)
{
    if (!error)
    {
        downstream_socket_.async_read_some(
            boost::asio::buffer(downstream_data_, max_data_length),
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
    if (!error)
    {
        async_write(downstream_socket_,
                    boost::asio::buffer(upstream_data_, bytes_transferred),
                    boost::bind(&bridge::handle_downstream_write,
                                shared_from_this(),
                                boost::asio::placeholders::error));
    }
    else
        close();
}

void bridge::close()
{
    boost::mutex::scoped_lock lock(mutex_);

    if (downstream_socket_.is_open())
    {
        downstream_socket_.close();
    }

    if (upstream_socket_.is_open())
    {
        upstream_socket_.close();
    }
}

bool bridge::acceptor::accept_connections()
{
    try
    {
        session_ = boost::shared_ptr<bridge>(new bridge(io_context_, upstream_host_));

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
        ip::tcp::resolver::query query(upstream_host_, "http");
        std::cout << "远端的域名 " << query.host_name() << std::endl;
        std::cout << "远端的服务 " << query.service_name() << std::endl;

        boost::asio::io_context ioc;
        ip::tcp::resolver resolver(ioc);
        ip::tcp::endpoint endpoint = resolver.resolve(query)->endpoint();
        std::cout << "远端的ip " << endpoint.address() << std::endl;
        std::cout << "远端的port " << endpoint.port() << std::endl;

        session_->start(endpoint);

        if (!accept_connections())
        {
            std::cerr << "Failure during call to accept." << std::endl;
        }
    }
    else
    {
        std::cerr << "Error: " << error.message() << std::endl;
    }
}
