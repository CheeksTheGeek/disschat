#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <taskflow/taskflow.hpp>
#include "common.hpp"

namespace Chat {

using tcp = boost::asio::ip::tcp;
using asio_context = boost::asio::io_context;

// Short aliases
static constexpr auto serialize   = ChatSerDes::serialize;
static constexpr auto deserialize = ChatSerDes::deserialize;
using Byte       = ChatSerDes::Byte;
using ByteBuffer = ChatSerDes::ByteBuffer;
using ByteView   = ChatSerDes::ByteView;
using Int32      = ChatSerDes::Int32;

class Session; // forward

/**
 * The main server that listens on a port, accepts new connections,
 * and broadcasts messages to the right “room” or all sessions.
 */
class Server {
public:
// optional ssl context
	Server(asio_context& io, const tcp::endpoint& ep);
	Server(asio_context& io, const tcp::endpoint& ep, boost::asio::ssl::context& ssl_context);
	void start_accept();

	// Multi-room broadcast
	void broadcastToRoom(const std::string& room, const ByteBuffer& data);

	// For demonstration, we store chat logs in memory for "search"
	// In a real system, you'd use a database.
	// Key: roomName -> vector of messages
	std::unordered_map<std::string, std::vector<ChatMessage>> _roomLogs;
	std::mutex _roomLogsMutex;

private:
	void accept();
	void build_taskflow_pipeline();

	asio_context& _io;
	tcp::acceptor _acceptor;
	std::mutex _sessionsMutex;
	std::unordered_set<std::shared_ptr<Session>> _sessions;
	boost::asio::ssl::context* _ssl_context{nullptr};

	// We'll track which sessions are in which room
	// roomName -> set of sessions
	std::unordered_map<std::string, std::unordered_set<std::shared_ptr<Session>>> _rooms;
	std::mutex _roomsMutex;

	tf::Taskflow _taskflow;
	tf::Executor _executor;
	bool _pipelineBuilt{false};

	friend class Session;
};

class Session : public std::enable_shared_from_this<Session> {
public:
	Session(tcp::socket socket,
					std::function<void(const ByteBuffer&, std::shared_ptr<Session>)> onMessage,
					std::function<void(std::shared_ptr<Session>)> onClose);

	void start();
	void send(const ByteBuffer& data);
	std::string get_address() const;

	// Get/set the current room name
	void setRoom(const std::string& room);
	std::string getRoom() const;

private:
	void read_header();
	void read_body(size_t length);
	void write();

	tcp::socket _socket;
	ByteBuffer _readBuffer;
	std::mutex _writeMutex;
	std::vector<ByteBuffer> _writeQueue;

	std::string _roomName; // current room name

	std::function<void(const ByteBuffer&, std::shared_ptr<Session>)> _onMessage;
	std::function<void(std::shared_ptr<Session>)> _onClose;
};

} // namespace Chat
