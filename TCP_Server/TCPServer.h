#pragma once
#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <thread>
#include <set>
#include "../nlohmann-json/json.hpp"

using boost::asio::ip::tcp;

class TCPServer
{
public:
    TCPServer(boost::asio::io_service &io_service);
    ~TCPServer();

    void start();
    void stopServer();
    void restartServer();

private:
    void acceptConnection();
    void handleCommunication();
    void receiveMessageFromClient();
    void sendMessageToClient();
    void handleError(const boost::system::error_code &ec);

    bool isCriticalError(const boost::system::error_code &ec);
    bool shouldRestart(const boost::system::error_code &ec);
    bool userPrefersRestart(const boost::system::error_code &ec);
    void loadConfiguration(const std::string &configFile);

private:
    boost::asio::io_service &m_io_service;
    tcp::acceptor m_acceptor;
    tcp::socket m_socket;
    bool is_running; // To monitor the server's operational status
    int error_count = 0;
    const int MAX_ERROR_THRESHOLD = 5; // An example error boundary

    struct ServerConfig
    {
        std::set<int> restartOnErrors; // A list of error code
        int maxConnectionLimit;
        int timeoutDuration;
    };

    ServerConfig serverConfig;
};