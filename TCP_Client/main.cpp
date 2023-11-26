#include "TCPClient.h"

int main()
{

    boost::asio::io_service io_service;
    TCPClient client(io_service);

    client.connect("127.0.0.1", 12345);

    std::thread client_thread([&]()
                              { client.startCommunication(); });

    client_thread.join();

    return 0;
}
