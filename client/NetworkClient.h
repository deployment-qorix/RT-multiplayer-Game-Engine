#pragma once
#include "protocol.h"
#include <boost/asio.hpp>
#include <thread>
#include <deque>
#include <mutex>

using boost::asio::ip::tcp;

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();
    void connect(const std::string& host, short port);
    void send(const GameMessage& msg);
    bool has_messages();
    GameMessage pop_message();
private:
    void run_service();
    void start_read();
    boost::asio::io_context io_context_;
    tcp::socket socket_;
    std::thread service_thread_;
    std::deque<GameMessage> incoming_messages_;
    std::mutex mutex_;
};