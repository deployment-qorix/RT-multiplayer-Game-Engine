#include "NetworkClient.h"
#include <iostream>
#include <cstring>

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

NetworkClient::NetworkClient(boost::asio::io_context& io)
    : io_context(io),
      tcp_socket(io),
      udp_socket(io) {}

void NetworkClient::connect(const std::string& host, const std::string& port) {
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(host, port);
    boost::asio::async_connect(tcp_socket, endpoints,
        [this, host](boost::system::error_code ec, tcp::endpoint) {
            if (!ec) {
                std::cout << "[Client] Connected to server via TCP.\n";
                do_read_header();
                start_udp(host);
            } else {
                std::cerr << "[Client] TCP Connect failed: " << ec.message() << "\n";
            }
        });
}

void NetworkClient::close() {
    boost::system::error_code ec;
    tcp_socket.close(ec);
    udp_socket.close(ec);
    if (ec) {
        std::cerr << "[Client] Error closing sockets: " << ec.message() << "\n";
    }
}

void NetworkClient::send_tcp(const GameMessage& msg) {
    boost::asio::post(io_context, [this, msg]() {
        bool write_in_progress = !write_msgs.empty();
        write_msgs.push_back(msg);
        if (!write_in_progress) {
            do_write();
        }
    });
}

void NetworkClient::send_udp(const UDPMessage& msg) {
    std::vector<char> buffer(sizeof(UDPMessage));
    std::memcpy(buffer.data(), &msg, sizeof(UDPMessage));

    udp_socket.async_send_to(boost::asio::buffer(buffer), server_udp_endpoint,
        [](boost::system::error_code ec, std::size_t /*bytes_sent*/) {
            if (ec) {
                std::cerr << "[Client] UDP send error: " << ec.message() << "\n";
            }
        });
}

void NetworkClient::do_read_header() {
    boost::asio::async_read(tcp_socket, boost::asio::buffer(read_msg_, header_length),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                uint32_t body_length;
                std::memcpy(&body_length, read_msg_, sizeof(uint32_t));
                do_read_body(body_length);
            } else {
                std::cerr << "[Client] TCP header read error: " << ec.message() << "\n";
                tcp_socket.close();
            }
        });
}

void NetworkClient::do_read_body(std::size_t body_length) {
    boost::asio::async_read(tcp_socket, boost::asio::buffer(read_msg_, body_length),
        [this, body_length](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                try {
                    GameMessage msg = GameMessage::deserialize(read_msg_); // ✅ fixed
                    {
                        std::lock_guard<std::mutex> lock(incoming_mutex);
                        incoming_tcp_messages.push_back(msg);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[Client] Failed to deserialize TCP message: " << e.what() << "\n";
                }
                do_read_header();
            } else {
                std::cerr << "[Client] TCP body read error: " << ec.message() << "\n";
                tcp_socket.close();
            }
        });
}

void NetworkClient::do_write() {
    if (write_msgs.empty()) return;

    auto buffer = write_msgs.front().serialize();
    uint32_t body_length = static_cast<uint32_t>(buffer.size());
    std::vector<char> header(sizeof(uint32_t));
    std::memcpy(header.data(), &body_length, sizeof(uint32_t));

    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(header));
    buffers.push_back(boost::asio::buffer(buffer));

    boost::asio::async_write(tcp_socket, buffers,
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                write_msgs.pop_front();
                if (!write_msgs.empty()) {
                    do_write();
                }
            } else {
                std::cerr << "[Client] TCP write error: " << ec.message() << "\n";
                tcp_socket.close();
            }
        });
}

void NetworkClient::start_udp(const std::string& host) {
    udp::resolver resolver(io_context);
    udp::resolver::results_type endpoints = resolver.resolve(udp::v4(), host, std::to_string(UDP_PORT));
    server_udp_endpoint = *endpoints.begin();

    udp_socket.open(udp::v4());

    // Receive buffer size = sizeof(UDPMessage)
    auto recv_buffer = std::make_shared<std::vector<char>>(sizeof(UDPMessage));
    udp_socket.async_receive_from(boost::asio::buffer(*recv_buffer), server_udp_endpoint,
        [this, recv_buffer, host](boost::system::error_code ec, std::size_t bytes_recvd) {
            if (!ec && bytes_recvd == sizeof(UDPMessage)) {
                try {
                    UDPMessage msg;
                    std::memcpy(&msg, recv_buffer->data(), sizeof(UDPMessage));
                    {
                        std::lock_guard<std::mutex> lock(incoming_mutex);
                        incoming_udp_messages.push_back(msg);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[Client] Failed to deserialize UDP message: " << e.what() << "\n";
                }
            } else if (ec) {
                std::cerr << "[Client] UDP receive error: " << ec.message() << "\n";
            }
            start_udp(host); // continue listening
        });
}
