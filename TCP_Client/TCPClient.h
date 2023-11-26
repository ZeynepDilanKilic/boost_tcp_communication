#include <iostream>
#include <boost/asio.hpp>


using boost::asio::ip::tcp;

class TCPClient {
public:
    TCPClient(boost::asio::io_service& io_service);
    ~TCPClient();

    void connect(const std::string& host, uint16_t port);
    void startCommunication();

private:
    void receiveMessageFromServer();
    void sendMessageToServer();

    boost::asio::io_service& m_io_service;
    tcp::socket m_socket;
};
