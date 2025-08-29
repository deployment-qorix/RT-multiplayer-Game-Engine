#pragma once

#include <boost/asio.hpp>
#include <deque>
#include <memory>
#include <mutex>
#include "../shared/protocol.h" // Include the shared message protocol

using boost::asio::ip::tcp;

class NetworkClient {
public:
    // Constructor: Initializes the client with an io_context.
    NetworkClient(boost::asio::io_context& io_context);

    // Connects to the server at the specified host and port.
    void connect(const std::string& host, const std::string& port);

    // Sends a game message to the server.
    void send(const GameMessage& msg);

    // Closes the connection to the server.
    void close();

    // A thread-safe queue for incoming messages from the server.
    // The main game loop will read from this queue.
    std::deque<GameMessage> incoming_messages;
    std::mutex incoming_mutex;

private:
    // Starts the asynchronous read loop to listen for messages.
    void do_read();

    // Handles the asynchronous writing of messages from the outgoing queue.
    void do_write();

    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    GameMessage read_msg_; // A buffer for a single incoming message
    std::deque<GameMessage> write_msgs_; // A queue for outgoing messages
};
