#include <iostream>
#include "server.hpp"
#include <boost/asio/ssl.hpp>

int main(int argc, char** argv) {
  using namespace Chat;

  try {
    unsigned short port = 5555;
    if(argc > 1) {
      port = static_cast<unsigned short>(std::stoi(argv[1]));
    }

    boost::asio::io_context ioc;
    
    // Create SSL context
    boost::asio::ssl::context ssl_context(boost::asio::ssl::context::tls_server);
    
    // Load server certificate and private key
    ssl_context.use_certificate_chain_file("server.crt");
    ssl_context.use_private_key_file("server.key", boost::asio::ssl::context::pem);
    
    // Optional: Load trusted CA certificates for client verification
    // ssl_context.load_verify_file("ca.crt");
    // ssl_context.set_verify_mode(boost::asio::ssl::verify_peer);

    Server server(ioc, tcp::endpoint(tcp::v4(), port), ssl_context);
    server.start_accept();

    std::cout << "[Server] Listening on port " << port << " (TLS enabled)" << std::endl;
    ioc.run();
  }
  catch(const std::exception& e) {
    std::cerr << "[Server] Exception: " << e.what() << std::endl;
  }
  return 0;
}
