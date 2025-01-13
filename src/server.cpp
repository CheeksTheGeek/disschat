#include "server.hpp"
#include <iostream>

namespace Chat {

// ~~~~~~~~~~~~~~~~~~~~ Session ~~~~~~~~~~~~~~~~~~~~

Session::Session(tcp::socket socket,
                 std::function<void(const ByteBuffer&, std::shared_ptr<Session>)> onMessage,
                 std::function<void(std::shared_ptr<Session>)> onClose)
: _socket(std::move(socket))
, _onMessage(std::move(onMessage))
, _onClose(std::move(onClose))
{
  // We begin with the standard read buffer sized for the “header” (Int32)
  _readBuffer.resize(sizeof(Int32));
  // Default room is "General"
  _roomName = "General";
}

void Session::start() {
  read_header();
}

void Session::send(const ByteBuffer& data) {
  {
    std::lock_guard<std::mutex> lock(_writeMutex);
    _writeQueue.push_back(data);
  }
  write();
}

std::string Session::get_address() const {
  try {
    return _socket.remote_endpoint().address().to_string();
  } catch(...) {
    return "unknown";
  }
}

void Session::setRoom(const std::string& room) {
  _roomName = room;
}
std::string Session::getRoom() const {
  return _roomName;
}

// ~~~ internals ~~~

void Session::read_header() {
  auto self = shared_from_this();
  boost::asio::async_read(_socket,
    boost::asio::buffer(_readBuffer),
    [this, self](boost::system::error_code ec, std::size_t bytesTransferred) {
      if(!ec && bytesTransferred == sizeof(Int32)) {
        Int32 length;
        std::memcpy(&length, _readBuffer.data(), sizeof(Int32));
        read_body(length);
      } else {
        // something is wrong, close
        _onClose(self);
      }
    }
  );
}

void Session::read_body(size_t length) {
  auto self = shared_from_this();
  _readBuffer.resize(length);
  boost::asio::async_read(_socket,
    boost::asio::buffer(_readBuffer),
    [this, self, length](boost::system::error_code ec, std::size_t bytesTransferred) {
      if(!ec && bytesTransferred == length) {
        _onMessage(_readBuffer, self);
        // go read next message
        _readBuffer.resize(sizeof(Int32));
        read_header();
      } else {
        _onClose(self);
      }
    }
  );
}

void Session::write() {
  auto self = shared_from_this();
  std::unique_lock<std::mutex> lock(_writeMutex);
  if(_writeQueue.empty()) return; // nothing to send
  auto data = _writeQueue.front();
  lock.unlock();

  Int32 len = (Int32)data.size();
  std::vector<boost::asio::const_buffer> buffers;
  buffers.push_back(boost::asio::buffer(&len, sizeof(len)));
  buffers.push_back(boost::asio::buffer(data));

  boost::asio::async_write(_socket,
    buffers,
    [this, self](boost::system::error_code ec, std::size_t /*bytesSent*/) {
      if(!ec) {
        std::lock_guard<std::mutex> guard(_writeMutex);
        _writeQueue.erase(_writeQueue.begin());
        if(!_writeQueue.empty()) {
          write();
        }
      } else {
        _onClose(self);
      }
    }
  );
}


// ~~~~~~~~~~~~~~~~~~~~ Server ~~~~~~~~~~~~~~~~~~~~

Server::Server(asio_context& io, const tcp::endpoint& ep)
: _io(io)
, _acceptor(io, ep)
{
  build_taskflow_pipeline();
}

Server::Server(asio_context& io, const tcp::endpoint& ep, boost::asio::ssl::context& ssl_context)
: _io(io)
, _acceptor(io, ep)
, _ssl_context(&ssl_context)
{
  build_taskflow_pipeline();
}

void Server::start_accept() {
  accept();
}

// “Rooms” feature: broadcast to only those sessions in a given room
void Server::broadcastToRoom(const std::string& room, const ByteBuffer& data) {
  std::lock_guard<std::mutex> lock(_roomsMutex);
  auto it = _rooms.find(room);
  if(it == _rooms.end()) {
    // no such room yet
    return;
  }
  for(auto& session : it->second) {
    session->send(data);
  }
}

void Server::accept() {
  _acceptor.async_accept(
    [this](boost::system::error_code ec, tcp::socket socket) {
      if(!ec) {
        std::cout << "[Server] Accepted new connection from "
                  << socket.remote_endpoint().address().to_string() << "\n";

        auto newSession = std::make_shared<Session>(
          std::move(socket),
          // onMessage
          [this](const ByteBuffer& data, std::shared_ptr<Session> sender){
            if (DEBUGGING) std::cout << "[Server] Raw message received from " << sender->get_address() 
                     << ", size: " << data.size() << " bytes" << std::endl;

            // Dispatch to taskflow for concurrency
            auto task = _taskflow.emplace([this, data, sender]() {
              try {
                ByteView view(data);
                auto msg = deserialize(view);

                if (DEBUGGING) std::cout << "[Server] Deserialized message:" << std::endl
                         << "  From: " << sender->get_address() << std::endl
                         << "  Sender: " << msg.sender << std::endl
                         << "  Room: " << sender->getRoom() << std::endl
                         << "  Content: " << msg.text << std::endl
                         << "  IsTyping: " << (msg.isTyping ? "yes" : "no") << std::endl
                         << "  IsFile: " << (msg.isFile ? "yes" : "no") << std::endl;

                // Maybe parse "/join RoomName" or "/search <keyword>" or "/typing ..."
                if(msg.isTyping) {
                  if (DEBUGGING) std::cout << "[Server] Handling typing notification" << std::endl;
                  // broadcast "Alice is typing…" to same room
                  ChatMessage typingNote;
                  typingNote.sender = "[System]";
                  typingNote.text   = msg.sender + " is typing...";
                  typingNote.room   = sender->getRoom();
                  ByteBuffer noteBuf = serialize(typingNote);
                  broadcastToRoom(sender->getRoom(), noteBuf);
                  return;
                }

                // If user typed `/join MyRoom`
                if(msg.text.rfind("/join ", 0) == 0) {
                  std::string newRoom = msg.text.substr(6); // skip "/join "
                  std::cout << "[Server] User " << msg.sender << " joining room: " << newRoom << std::endl;
                  // remove from old room, add to new
                  {
                    std::lock_guard<std::mutex> lock(_roomsMutex);
                    _rooms[sender->getRoom()].erase(sender);
                    _rooms[newRoom].insert(sender);
                  }
                  sender->setRoom(newRoom);

                  // send system message to user
                  ChatMessage systemMsg;
                  systemMsg.sender = "[System]";
                  systemMsg.text   = "You joined room: " + newRoom;
                  systemMsg.room   = newRoom;
                  ByteBuffer sysBuf = serialize(systemMsg);
                  sender->send(sysBuf);
                  return;
                }

                // If user typed `/search <keyword>`
                if(msg.text.rfind("/search ", 0) == 0) {
                  std::string kw = msg.text.substr(8);
                  if (DEBUGGING) std::cout << "[Server] User " << msg.sender << " searching for: " << kw << std::endl;
                  // search in the current room logs
                  std::vector<ChatMessage> results;
                  {
                    std::lock_guard<std::mutex> lock(_roomLogsMutex);
                    auto it = _roomLogs.find(sender->getRoom());
                    if(it != _roomLogs.end()) {
                      for(const auto& m : it->second) {
                        if(m.text.find(kw) != std::string::npos) {
                          results.push_back(m);
                        }
                      }
                    }
                  }
                  if (DEBUGGING) std::cout << "[Server] Found " << results.size() << " matches for search" << std::endl;
                  // send the results back to the user
                  ChatMessage searchResult;
                  searchResult.sender = "[System]";
                  searchResult.room   = sender->getRoom();
                  // combine found messages
                  if(results.empty()) {
                    searchResult.text = "No matches found for: " + kw;
                  } else {
                    searchResult.text = "Matches:\n";
                    for(const auto& rm : results) {
                      searchResult.text += rm.sender + ": " + rm.text + "\n";
                    }
                  }
                  auto rbuf = serialize(searchResult);
                  sender->send(rbuf);
                  return;
                }

                // If isFile == true, we handle file broadcasts
                if(msg.isFile) {
                  if (DEBUGGING) std::cout << "[Server] Broadcasting file from " << msg.sender << std::endl;
                  broadcastToRoom(sender->getRoom(), data);
                } else {
                  if (DEBUGGING) std::cout << "[Server] Broadcasting text message to room: " << sender->getRoom() << std::endl;
                  // Normal text message
                  // 1) Save to in-memory logs
                  {
                    std::lock_guard<std::mutex> lock(_roomLogsMutex);
                    _roomLogs[sender->getRoom()].push_back(msg);
                  }
                  // 2) Broadcast to the same room
                  broadcastToRoom(sender->getRoom(), data);
                }

              } catch(const std::exception& e) {
                std::cerr << "[Server] Deserialization error: " << e.what() << std::endl;
              }
            });
            _executor.run(_taskflow);
          },
          // onClose
          [this](std::shared_ptr<Session> s){
            std::cout << "[Server] Handling connection close for " << s->get_address() << std::endl;
            // remove from session set
            {
              std::lock_guard<std::mutex> lock(_sessionsMutex);
              _sessions.erase(s);
            }
            // also remove from _rooms
            {
              std::lock_guard<std::mutex> lock2(_roomsMutex);
              _rooms[s->getRoom()].erase(s);
            }
            std::cout << "[Server] Connection closed and cleanup complete for " << s->get_address() << "\n";
          }
        );

        // Add new session
        {
          std::lock_guard<std::mutex> lock(_sessionsMutex);
          _sessions.insert(newSession);
          std::cout << "[Server] Added new session, total sessions: " << _sessions.size() << std::endl;
        }
        // Add session to default "General" room
        {
          std::lock_guard<std::mutex> lock(_roomsMutex);
          _rooms["General"].insert(newSession);
          std::cout << "[Server] Added session to General room, room size: " << _rooms["General"].size() << std::endl;
        }
        newSession->start();
      }
      accept(); // keep accepting new connections
    }
  );
}

void Server::build_taskflow_pipeline() {
  if(_pipelineBuilt) return;
  _pipelineBuilt = true;

  // e.g. you could create a tf::ScalablePipeline for advanced concurrency
  // or keep it simple. We'll just keep a single graph for message tasks.
  _taskflow.name("ServerMessageProcessing");
}

} // namespace Chat
