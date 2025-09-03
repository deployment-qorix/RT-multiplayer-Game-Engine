// client3d/NetworkClient.h
#pragma once
#include <boost/asio.hpp>
#include <thread>
#include <deque>
#include <mutex>
#include "../shared/protocol.h"   // ✅ make sure this path is correct

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

class NetworkClient {
public:
    explicit NetworkClient(boost::asio::io_context& io);

    void connect(const std::string& host, const std::string& port);
    void close();

    void send_tcp(const GameMessage& msg);
    void send_udp(const UDPMessage& msg);

    void set_id(uint32_t id) { my_id = id; }
    uint32_t get_id() const { return my_id; }

    // Queues for incoming messages
    std::deque<GameMessage> incoming_tcp_messages;
    std::deque<UDPMessage> incoming_udp_messages;
    std::mutex incoming_mutex;

private:
    void do_read_header();
    void do_read_body(std::size_t body_length);
    void do_write();

    // UDP
    void start_udp(const std::string& host);

    boost::asio::io_context& io_context;
    tcp::socket tcp_socket;
    udp::socket udp_socket;
    udp::endpoint server_udp_endpoint;

    std::deque<GameMessage> write_msgs;
    enum { header_length = sizeof(uint32_t) };
    char read_msg_[2048];  // ✅ increased from 512 → 2048 for safety

    uint32_t my_id = 0;
};
