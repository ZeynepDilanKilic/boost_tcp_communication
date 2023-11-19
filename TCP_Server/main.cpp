#include "TCPServer.h"
#include <thread>

int main()
{
    boost::asio::io_service io_service;
    TCPServer server(io_service);
    std::thread server_thread([&]()
                              { server.start(); });
   
    server_thread.join();

    return 0;
}
