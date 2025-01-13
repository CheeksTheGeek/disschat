#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>

#include <boost/asio.hpp>
#include <taskflow/taskflow.hpp>
#include "common.hpp"

namespace Chat {

using tcp = boost::asio::ip::tcp;
using asio_context = boost::asio::io_context;

static constexpr auto serialize   = ChatSerDes::serialize;
static constexpr auto deserialize = ChatSerDes::deserialize;
using ByteBuffer = ChatSerDes::ByteBuffer;
using ByteView   = ChatSerDes::ByteView;
using Int32      = ChatSerDes::Int32;

/**
 * A single chat client that can connect to the server, send messages,
 * and callback on incoming messages.
 */
class Client {
public:
  Client();
  ~Client();

  // Connect to the server at host:port
  void connect(const std::string& host, unsigned short port);

  // Send a chat message
  void send(const ChatMessage& msg);

  // Called when a new message arrives
  std::function<void(const ChatMessage&)> on_incoming_message;

private:
  void _connect(const std::string& host, unsigned short port);
  void read_header();
  void read_body(size_t length);
  void write();

  asio_context _io_context;
  tcp::socket  _socket;
  std::thread  _io_thread;

  tf::Taskflow _taskflow;
  tf::Executor _executor;

  bool _connected{false};
  std::mutex _write_mutex;
  std::vector<ByteBuffer> _write_queue;
  ByteBuffer _read_buffer;
};

} // end namespace Chat
