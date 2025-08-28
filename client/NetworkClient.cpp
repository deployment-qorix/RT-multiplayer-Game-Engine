#include "NetworkClient.h"
#include <iostream>
NetworkClient::NetworkClient() : socket_(io_context_) {}
NetworkClient::~NetworkClient() {
    io_context_.stop();
    if (service_thread_.joinable()) {
        service_thread_.join();
    }
}
void NetworkClient::connect(const std::string& host, short port) {
    tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(host, std::to_string(port));
    boost::asio::connect(socket_, endpoints);
    service_thread_ = std::thread([this]() { run_service(); });
}
void NetworkClient::send(const GameMessage& msg) {
    boost::asio::post(io_context_, [this, msg]() {
        boost::asio::async_write(socket_, boost::asio::buffer(&msg, sizeof(GameMessage)),
            [](boost::system::error_code /*ec*/, std::size_t /*length*/) {});
    });
}
bool NetworkClient::has_messages() {
    std::lock_guard<std::mutex> lock(mutex_);
    return !incoming_messages_.empty();
}
GameMessage NetworkClient::pop_message() {
    std::lock_guard<std::mutex> lock(mutex_);
    GameMessage msg = incoming_messages_.front();
    incoming_messages_.pop_front();
    return msg;
}
void NetworkClient::run_service() {
    start_read();
    io_context_.run();
}
void NetworkClient::start_read() {
    GameMessage* msg = new GameMessage();
    boost::asio::async_read(socket_, boost::asio::buffer(msg, sizeof(GameMessage)),
        [this, msg](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (incoming_messages_.size() < 1000) {
                        incoming_messages_.push_back(*msg);
                    }
                }
                delete msg;
                start_read();
            } else {
                delete msg;
            }
        });
}