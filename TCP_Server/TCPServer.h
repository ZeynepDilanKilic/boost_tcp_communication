#pragma once
#include "common/FramedConnection.h"
#include <set>

class TCPServer {
public:
    explicit TCPServer(boost::asio::io_context& io, std::uint16_t port = 12345,
                       const std::string& config_file = "");
    ~TCPServer();
    void start();
    void stopServer();
    void restartServer();

private:
    class Session;
    void acceptConnection();
    void loadConfiguration(const std::string& path);

    boost::asio::io_context& io_;
    tcp_demo::tcp::acceptor acceptor_;
    boost::asio::steady_timer accept_retry_;
    std::set<std::shared_ptr<Session>> sessions_;
    std::uint16_t port_;
    std::size_t max_connections_ = 5;
    std::chrono::milliseconds timeout_{5000};
    std::uint64_t generation_ = 0;
    bool running_ = false;
};
