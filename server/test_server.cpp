#include <iostream>
#include <string>
#include <memory>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        std::cout << "[Test Server] Client connected. Sending welcome message..." << std::endl;
        std::string welcome_message = "Hello from server!\n";
        
        auto self = shared_from_this();
        boost::asio::async_write(socket_, boost::asio::buffer(welcome_message),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::cout << "[Test Server] Welcome message sent successfully." << std::endl;
                } else {
                    std::cerr << "[Test Server] Write error: " << ec.message() << std::endl;
                }
            });
    }

private:
    tcp::socket socket_;
};

class Server {
public:
    Server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        boost::asio::io_context io_context;
        Server server(io_context, 1337);
        std::cout << "[Test Server] Listening on port 1337..." << std::endl;
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}