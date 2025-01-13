#include <iostream>

#include "server.hpp"

int main(int argc, char** argv) {
  try {
    unsigned short port = 5555;
    if(argc > 1) {
      port = static_cast<unsigned short>(std::stoi(argv[1]));
    }

    boost::asio::io_context ioc;
    ChatServer server(ioc, port);
    server.startAccept();

    std::cout << "[Server] Listening on port " << port << "\n";
    ioc.run();
  }
  catch(std::exception& e) {
    std::cerr << "[Server] Exception: " << e.what() << "\n";
  }
  return 0;
}
