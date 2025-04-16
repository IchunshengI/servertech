#include <cstdlib>
#include <cstddef>
#include <string>

#include "https_proxy.hpp"
namespace proxy = https_proxy;

int main(int argc, char *argv[])
{
   if (argc != 4)
   {
      std::cerr << "usage: proxy_server <local host ip> <local port> <forward host>" << std::endl;
      return 1;
   }

   const std::string local_host = argv[1];
   const std::string forward_host = argv[3];
   const unsigned short local_port = static_cast<unsigned short>(::atoi(argv[2]));

   try
   {
      boost::asio::io_context ioc;
#ifdef INCLUDE_HTTPS_REQUEST_CONTEXT_HPP
#define INCLUDE_HTTPS_REQUEST_CONTEXT_HPP
      // SSL/TLS
      boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
      ctx.set_default_verify_paths(); // 设置 SSL 上下文选项
      ctx.set_verify_mode(boost::asio::ssl::verify_peer);
      proxy::bridge::acceptor acceptor(ioc, ctx,
                                       local_host, local_port,
                                       forward_host);
#else
      proxy::bridge::acceptor acceptor(ioc,
                                       local_host, local_port,
                                       forward_host);
#endif

      acceptor.accept_connections();
      ioc.run();
   }
   catch (std::exception &e)
   {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}
