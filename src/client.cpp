#include <iostream>

#include "client.hpp"

namespace Chat {

Client::Client() : _socket(_io_context) {
    // may build the concurrency graph for the taskflow
}

Client::~Client() {
    _io_context.stop();
    if(_io_thread.joinable()) _io_thread.join();
}

void Client::connect(const std::string& host, unsigned short port) {
    // start any initial tasks ()
    _executor.run(_taskflow);

    // kick off the async connect operation
    _connect(host, port);

    // launch the IO context in background
    _io_thread = std::thread([this] { 
        try {
            _io_context.run();
        } catch(const std::exception& e) {
            std::cerr << "[Client] IO Context exception: " << e.what() << std::endl;
        }
    });
}

void Client::send(const ChatMessage& message) {
    // serialize the message
    ByteBuffer message_bytes = serialize(message);

    // enqueue the message for sending
    {
        std::lock_guard lock(_write_mutex);
        _write_queue.push_back(message_bytes);
    }

    if (_connected) {
        // kick off the async write operation
        write();
    }
}

void Client::_connect(const std::string& host, unsigned short port) {
    // resolve the host and port
    tcp::resolver resolver(_io_context);
    auto endpoints = resolver.resolve(host, std::to_string(port));

    boost::asio::async_connect(_socket, endpoints, 
        [this](const boost::system::error_code& ec, const tcp::endpoint& endpoint) { // this is a lambda function being given as a callback
            // the lambda is taking this as a capture, which is the Client object and the error code and endpoint as arguments
            // in C++, a lambda capture is a way to pass variables from the enclosing scope to the lambda function
            if (ec) std::cerr << "[Client] Connection error: " << ec.message() << std::endl;
            else {
                std::cout << "[Client] Connected to " << endpoint.address().to_string() << ":" << endpoint.port() << std::endl;
                _connected = true;
                _read_buffer.resize(sizeof(Int32));
                read_header();
            }
        }
    );
}

void Client::read_header() {
    boost::asio::async_read(_socket, boost::asio::buffer(_read_buffer), 
        [this](const boost::system::error_code& ec, size_t bytes_transferred) {
            if (!ec && bytes_transferred == sizeof(Int32)) {
                Int32 body_length;
                std::memcpy(&body_length, _read_buffer.data(), sizeof(Int32));
                read_body(body_length);
            } else (_connected = false, std::cerr << "[Client] Read header error: " << ec.message() << std::endl);
        }
    );
}

void Client::read_body(size_t length) {
    _read_buffer.resize(length);
    boost::asio::async_read(_socket, boost::asio::buffer(_read_buffer), 
        [this, length](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred == length) {
                try {
                    ByteView view(_read_buffer);
                    ChatMessage chat_message = deserialize(view);
                    if (on_incoming_message) on_incoming_message(chat_message);
                } catch(const std::exception& e) {std::cerr << "[Client] Deserialization error: " << e.what() << std::endl;}
                _read_buffer.resize(sizeof(Int32));
                read_header();
            } else (_connected = false, std::cerr << "[Client] Read body error: " << ec.message() << std::endl);
        }
    );
}

void Client::write() {
    std::unique_lock<std::mutex> lock(_write_mutex);
    if (_write_queue.empty() || !_connected) return;

    auto data = _write_queue.front();
    lock.unlock();

    Int32 data_length = static_cast<Int32>(data.size());

    std::vector<asio_constant_buffer> buffers;
    buffers.push_back(boost::asio::buffer(&data_length, sizeof(Int32)));
    buffers.push_back(boost::asio::buffer(data));

    boost::asio::async_write(_socket, buffers, 
        [this](const boost::system::error_code& ec, std::size_t){
            if (!ec) {
                std::lock_guard<std::mutex> guard(_write_mutex);
                _write_queue.erase(_write_queue.begin()); 
                if (!_write_queue.empty()) write();
            } else (_connected = false, std::cerr << "[Client] Write error: " << ec.message() << std::endl);
        }
    );
}


} // namespace Chat