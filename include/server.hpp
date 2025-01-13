#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <unordered_set>
#include <mutex>

#include <boost/asio.hpp>
#include <taskflow/taskflow.hpp>

#include "common.hpp"

namespace Chat {
    using tcp = boost::asio::ip::tcp;
    using asio_context = boost::asio::io_context;
    using asio_constant_buffer = boost::asio::const_buffer;
    
    static constexpr auto serialize   = ChatSerDes::serialize;    // serialize(const T& value) -> ByteBuffer
    static constexpr auto deserialize = ChatSerDes::deserialize;          // deserialize(ByteView& bytes, T& value) -> ChatMessage
    using Byte          = ChatSerDes::Byte;         // std::uint8_t
    using Int32         = ChatSerDes::Int32;        // std::uint32_t
    using ByteBuffer    = ChatSerDes::ByteBuffer;   // std::vector<Byte>
    using ByteView      = ChatSerDes::ByteView;     // span<const Byte>
    
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket,
                std::function<void(const ByteBuffer&, std::shared_ptr<Session>)> on_message,
                std::function<void(std::shared_ptr<Session>)> on_close)
            : _socket(std::move(socket))
            , _on_message(on_message)
            , _on_close(on_close)
            , _read_buffer(sizeof(Int32)) {}
        void start();
        void send(const ByteBuffer& message);
        std::string get_address() const;
        
        void set_room(const std::string& room) {_roomName = room;}
        std::string get_room() const           {return _roomName;}
    private:
        std::string _roomName{"General"}; 
        void read_header();
        void read_body(size_t length);
        void write();

        tcp::socket _socket;
        ByteBuffer _read_buffer;
        std::mutex _write_mutex;
        std::vector<ByteBuffer> _write_queue;

        std::function<void(const ByteBuffer&, std::shared_ptr<Session>)> _on_message;
        std::function<void(std::shared_ptr<Session>)> _on_close;
    };

    class Server {
    public:
        Server(asio_context& io_context, const tcp::endpoint& endpoint)
            : _io_context(io_context)
            , _acceptor(io_context, endpoint) { build_taskflow_pipeline(); }
        
        void start_accept() { accept(); }
        void broadcast(const ByteBuffer& message);
        void broadcast_to_room(const std::string& room, const ByteBuffer& message);

    private:
        void accept();
        void build_taskflow_pipeline();

        asio_context& _io_context;
        tcp::acceptor _acceptor;
        std::mutex _sessions_mutex;
        std::unordered_set<std::shared_ptr<Session>> _sessions;

        std::unordered_map<std::string, std::unordered_set<std::shared_ptr<Session>>> _rooms;
        std::mutex _roomsMutex;


        tf::Taskflow _taskflow;
        tf::Executor _executor;

        bool _pipeline_built{false};

    };

} // namespace chat

