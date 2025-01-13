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
    using asio_context = boost::asio::io_context;
    using asio_constant_buffer = boost::asio::const_buffer;
    using tcp = boost::asio::ip::tcp;
    using socket = boost::asio::ip::tcp::socket;
    
    using serialize = ChatSerDes::serialize;
    using deserialize = ChatSerDes::deserialize;
    using Byte = ChatSerDes::Byte;
    using Int32 = ChatSerDes::Int32;
    using ByteBuffer = ChatSerDes::ByteBuffer;
    using ByteView = ChatSerDes::ByteView;
    

    class Client {
    public:
        Client();
        ~Client();

        void connect(const std::string& host, unsigned short port);
        void send(const ChatMessage& message);

        // callback for whenever a new message is received
        std::function<void(const ChatMessage&)> on_incoming_message;
    private:
        void _connect(const std::string& host, unsigned short port);
        void read_header();
        void read_body(size_t length);
        void write();

        asio_context _io_context;
        socket _socket;
        std::thread _io_thread;
        std::mutex _write_mutex;
        std::vector<ByteBuffer> _write_queue;
        ByteBuffer _read_buffer;

        tf::Taskflow _taskflow;
        tf::Executor _executor;

        bool _connected = false;
    }
}