#include "NetworkClient.h"
#include <iostream>

NetworkClient::NetworkClient(boost::asio::io_context& io_context)
    : io_context_(io_context), socket_(io_context) {}

void NetworkClient::connect(const std::string& host, const std::string& port) {
    auto endpoints = tcp::resolver(io_context_).resolve(host, port);
    boost::asio::async_connect(socket_, endpoints,
        [this](boost::system::error_code ec, tcp::endpoint) {
            if (!ec) {
                std::cout << "[Client] Connected to server." << std::endl;
                do_read(); // Start listening for messages once connected
            } else {
                std::cerr << "[Client] Connection failed: " << ec.message() << std::endl;
            }
        });
}

void NetworkClient::send(const GameMessage& msg) {
    // Post the write operation to the io_context to ensure thread safety
    boost::asio::post(io_context_, [this, msg]() {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            do_write();
        }
    });
}

void NetworkClient::close() {
    // Post the close operation to the io_context
    boost::asio::post(io_context_, [this]() {
        socket_.close();
    });
}

void NetworkClient::do_read() {
    boost::asio::async_read(socket_,
        boost::asio::buffer(&read_msg_, sizeof(GameMessage)),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // Lock the mutex, add the message to the queue, and then unlock
                std::lock_guard<std::mutex> lock(incoming_mutex);
                incoming_messages.push_back(read_msg_);
                
                do_read(); // Continue the read loop
            } else {
                std::cerr << "[Client] Read error: " << ec.message() << std::endl;
                socket_.close();
            }
        });
}

void NetworkClient::do_write() {
    boost::asio::async_write(socket_,
        boost::asio::buffer(&write_msgs_.front(), sizeof(GameMessage)),
        [this](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty()) {
                    do_write(); // Continue writing if there are more messages
                }
            } else {
                std::cerr << "[Client] Write error: " << ec.message() << std::endl;
                socket_.close();
            }
        });
}
