#include "client.hpp"
#include <iostream>

namespace Chat {

Client::Client()
: _socket(_io_context)
{
  // You could build advanced concurrency in _taskflow here
}

Client::~Client() {
  _io_context.stop();
  if(_io_thread.joinable()) {
    _io_thread.join();
  }
}

void Client::connect(const std::string& host, unsigned short port) {
  // run any initial tasks
  _executor.run(_taskflow);

  // start async connect
  _connect(host, port);

  // run io_context in background
  _io_thread = std::thread([this] {
    try {
      _io_context.run();
    } catch(const std::exception& e) {
      std::cerr << "[Client] IO Context exception: " << e.what() << std::endl;
    }
  });
}

void Client::send(const ChatMessage& msg) {
  // Serialize
  if (DEBUGGING) std::cout << "called send function with message: " << msg.text << std::endl;
  ByteBuffer buf = serialize(msg);
  if (DEBUGGING) std::cout << "serialized" << std::endl;
  // queue for sending
  {
    std::lock_guard<std::mutex> guard(_write_mutex);
    _write_queue.push_back(std::move(buf));
  }

  if(_connected) {
    write();
  }
}

void Client::_connect(const std::string& host, unsigned short port) {
  tcp::resolver resolver(_io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port));

  boost::asio::async_connect(_socket, endpoints,
    [this](const boost::system::error_code& ec, const tcp::endpoint& ep) {
      if(!ec) {
        std::cout << "[Client] Connected to " << ep.address().to_string()
                  << ":" << ep.port() << std::endl;
        _connected = true;
        // start reading
        _read_buffer.resize(sizeof(Int32));
        read_header();
      } else {
        std::cerr << "[Client] Connect error: " << ec.message() << std::endl;
      }
    }
  );
}

void Client::read_header() {
  boost::asio::async_read(_socket, boost::asio::buffer(_read_buffer),
    [this](const boost::system::error_code& ec, size_t bytesTransferred) {
      if(!ec && bytesTransferred == sizeof(Int32)) {
        Int32 bodyLen;
        std::memcpy(&bodyLen, _read_buffer.data(), sizeof(Int32));
        read_body(bodyLen);
      } else {
        _connected = false;
        std::cerr << "[Client] Read header error: " << ec.message() << std::endl;
      }
    }
  );
}

void Client::read_body(size_t length) {
  _read_buffer.resize(length);
  boost::asio::async_read(_socket, boost::asio::buffer(_read_buffer),
    [this, length](const boost::system::error_code& ec, size_t transferred) {
      if(!ec && transferred == length) {
        try {
          ByteView view(_read_buffer);
          auto msg = deserialize(view);
          if(on_incoming_message) {
            on_incoming_message(msg);
          }
        } catch(const std::exception& e) {
          std::cerr << "[Client] Deserialization error: " << e.what() << std::endl;
        }
        // read next
        _read_buffer.resize(sizeof(Int32));
        read_header();
      } else {
        _connected = false;
        std::cerr << "[Client] Read body error: " << ec.message() << std::endl;
      }
    }
  );
}

void Client::write() {
  if (DEBUGGING) std::cout << "called write function" << std::endl;
  std::unique_lock<std::mutex> lock(_write_mutex);
  if(_write_queue.empty() || !_connected) {
    return;
  }
  auto data = _write_queue.front();
  lock.unlock();

  Int32 len = (Int32)data.size();
  std::vector<boost::asio::const_buffer> bufs;
  bufs.push_back(boost::asio::buffer(&len, sizeof(len)));
  bufs.push_back(boost::asio::buffer(data));

  boost::asio::async_write(_socket, bufs,
    [this](const boost::system::error_code& ec, std::size_t) {
      if(!ec) {
        std::lock_guard<std::mutex> guard(_write_mutex);
        _write_queue.erase(_write_queue.begin());
        if(!_write_queue.empty()) {
          write();
        }
      } else {
        _connected = false;
        std::cerr << "[Client] Write error: " << ec.message() << std::endl;
      }
    }
  );
}


} // namespace Chat