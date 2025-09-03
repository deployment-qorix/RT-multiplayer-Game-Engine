#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

int main() {
    try {
        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);

        boost::asio::connect(socket, resolver.resolve("localhost", "1337"));
        std::cout << "[Test Client] Connected to server." << std::endl;

        boost::asio::streambuf buffer;
        boost::asio::read_until(socket, buffer, '\n');
        
        std::istream is(&buffer);
        std::string message;
        std::getline(is, message);

        std::cout << "[Test Client] Message from server: " << message << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Test Client] Exception: " << e.what() << std::endl;
    }

    std::cout << "[Test Client] Test finished." << std::endl;
    return 0;
}