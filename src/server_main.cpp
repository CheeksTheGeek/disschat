#include <iostream>

#include "server.hpp"

int main(int argc, char** argv) {
  using namespace Chat;
  try {
    unsigned short port = 5555;
    if(argc > 1) {
      port = static_cast<unsigned short>(std::stoi(argv[1]));
    }

    asio_context ioc;
    Server server(ioc, tcp::endpoint(tcp::v4(), port));
    server.start_accept();

    std::cout << "[Server] Listening on port " << port << "\n";
    ioc.run();
  }
  catch(std::exception& e) {
    std::cerr << "[Server] Exception: " << e.what() << "\n";
  }
  return 0;
}
