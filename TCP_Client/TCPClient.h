#pragma once
#include "common/FramedConnection.h"
#include "common/LatencyStats.h"
#include "nlohmann-json/json.hpp"

struct ClientOptions {
    std::uint64_t message_count = 10; // Zero runs until interrupted.
    std::size_t payload_size = 64;
    std::chrono::milliseconds interval{1000};
    std::chrono::milliseconds timeout{3000};
};

class TCPClient {
public:
    explicit TCPClient(boost::asio::io_context& io, ClientOptions options = {});
    ~TCPClient();
    void connect(const std::string& host, std::uint16_t port);
    void startCommunication();
    void stop();
    nlohmann::json statistics() const;
    std::function<void()> on_finished;

private:
    using Clock = std::chrono::steady_clock;
    void beginConnection();
    void exchangeMessage();
    void retry(tcp_demo::ErrorCode ec);
    bool current(std::uint64_t attempt) const { return running_ && attempt == attempt_; }

    boost::asio::io_context& io_;
    tcp_demo::tcp::resolver resolver_;
    boost::asio::steady_timer connect_deadline_;
    boost::asio::steady_timer schedule_;
    std::shared_ptr<tcp_demo::FramedConnection> connection_;
    ClientOptions options_;
    std::string host_ = "127.0.0.1";
    std::string port_ = "12345";
    std::string message_;
    std::chrono::milliseconds retry_delay_{100};
    Clock::time_point started_{};
    Clock::time_point finished_{};
    Clock::time_point request_started_{};
    tcp_demo::LatencyStats latencies_;
    std::uint64_t attempt_ = 0;
    std::uint64_t messages_ = 0;
    std::uint64_t connections_ = 0;
    std::uint64_t failures_ = 0;
    bool running_ = false;
    bool started_once_ = false;
};
