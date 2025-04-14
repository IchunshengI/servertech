#include "tcp_proxy.hpp"

using namespace tcp_proxy;

void bridge::start(const std::string &upstream_host, unsigned short upstream_port)
{
    upstream_socket_.async_connect(
        ip::tcp::endpoint(
            boost::asio::ip::address::from_string(upstream_host),
            upstream_port),
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
        async_write(upstream_socket_,
                    boost::asio::buffer(downstream_data_, bytes_transferred),
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
        session_ = boost::shared_ptr<bridge>(new bridge(io_context_));
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
        session_->start(upstream_host_, upstream_port_);

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
