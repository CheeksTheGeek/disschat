#include <iostream>

#include "server.hpp"

namespace Chat{

// Session's constructor inlined in header

void Session::start() {read_header();}

void Session::send(const ByteBuffer& message) {
    {
        std::lock_guard<std::mutex> lock(_write_mutex);
        _write_queue.push_back(message);
    }
    write();
}

std::string Session::get_address() const {
    try {
        return _socket.remote_endpoint().address().to_string();
    } catch (...) { /// ... is a catch-all exception handler
        return "Unknown";
    }
}

void Session::read_header() {
    auto self(shared_from_this());
    boost::asio::async_read(_socket, boost::asio::buffer(_read_buffer),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred == sizeof(Int32)) {
                Int32 length;
                std::memcpy(&length, _read_buffer.data(), sizeof(Int32));
                read_body(length);
            } else _on_close(self);
        }
    );
}

void Session::read_body(size_t length) {
    auto self(shared_from_this());
    _read_buffer.resize(length);
    boost::asio::async_read(_socket, boost::asio::buffer(_read_buffer),
        [this, self, length](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred == length) {
                _on_message(_read_buffer, self);
                _read_buffer.resize(sizeof(Int32));
                read_header();
            } else _on_close(self);
        }
    );
}

void Session::write() {
    auto self(shared_from_this());

    std::unique_lock<std::mutex> lock(_write_mutex);
    if (_write_queue.empty()) return;
    auto data = std::move(_write_queue.front());
    lock.unlock();

    Int32 data_length = static_cast<Int32>(data.size());
    
    std::vector<asio_constant_buffer> buffers;
    buffers.push_back(boost::asio::buffer(&data_length, sizeof(Int32)));
    buffers.push_back(boost::asio::buffer(data));

    boost::asio::async_write(_socket, buffers,
        [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                std::lock_guard<std::mutex> guard(_write_mutex);
                _write_queue.erase(_write_queue.begin());
                if (!_write_queue.empty()) write();
            } else _on_close(self);
        }
    );
}

// Server's constructor inlined in header

void Server::broadcast(const ByteBuffer& message) {
    std::lock_guard<std::mutex> lock(_sessions_mutex);
    for (auto& session : _sessions) session->send(message);
}

void Server::accept() {
    _acceptor.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "[Server] New connection from " << socket.remote_endpoint().address().to_string() << std::endl;

                auto session = std::make_shared<Session>(
                    std::move(socket),
                    [this](const ByteBuffer& data, std::shared_ptr<Session> sender) {
                    // dispatch to taskflow for concurrency
                    auto task = _taskflow.emplace([this, data, sender]() {
                        try {
                            auto message = deserialize(data);
                            std::cout << "[Server] " << sender->remoteAddress() << " => " << message.sender << ": " << message.text << std::endl;
                            broadcast(serialize(message));
                        } catch (const std::exception& e) {
                            std::cerr << "[Server] Deserialization error: " << e.what() << std::endl;
                        }
                    });
                    _executor.run(_taskflow);
                },
                [this](std::shared_ptr<Session> session) {
                    {
                        std::lock_guard<std::mutex> lock(_sessions_mutex);
                        _sessions.erase(session);
                    }
                    std::cout << "[Server] Connection closed by " << session->get_address() << std::endl;
                }
                );
                {
                    std::lock_guard<std::mutex> lock(_sessions_mutex);
                    _sessions.insert(session);
                }
                session->start();
            }
            accept();
        }
    );
}

void Server::build_taskflow_pipeline() {
    if (_pipeline_built) return;
    _pipeline_built = true;

    // TODO: create an actual pipeline
    _taskflow.name("ServerMessageProcessing");
}


} // namespace Chat