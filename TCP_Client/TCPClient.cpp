#include "TCPClient.h"
#include <thread>

TCPClient::TCPClient(boost::asio::io_service &io_service)
    : m_io_service(io_service), m_socket(io_service) {}

TCPClient::~TCPClient() {
    m_socket.close();
}

void TCPClient::connect(const std::string &host, uint16_t port) {
    boost::system::error_code ec;
    m_socket.connect(tcp::endpoint(boost::asio::ip::address::from_string(host), port), ec);
    if (ec) {
        std::cerr << "Connection error: " << ec.message() << std::endl;
        // Handle connection error
    }
}

void TCPClient::startCommunication() {
    while (true) {
        sendMessageToServer();
        receiveMessageFromServer();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void TCPClient::receiveMessageFromServer() {
    boost::system::error_code ec;
    size_t size;

    m_socket.read_some(boost::asio::buffer(&size, sizeof(size)), ec);
    if (!ec && size > 0) {
        std::vector<char> incoming_data(size);
        m_socket.read_some(boost::asio::buffer(incoming_data), ec);
        if (!ec) {
            std::cout << "Received message: " << std::string(incoming_data.begin(), incoming_data.end()) << std::endl;
        } else {
            std::cerr << "Receive error: " << ec.message() << std::endl;
        }
    } else {
        std::cerr << "Read error: " << ec.message() << std::endl;
    }
}

void TCPClient::sendMessageToServer() {
    std::string message = "Hello from Client";
    size_t size = message.size();
    boost::asio::write(m_socket, boost::asio::buffer(&size, sizeof(size)));
    boost::asio::write(m_socket, boost::asio::buffer(message));
}