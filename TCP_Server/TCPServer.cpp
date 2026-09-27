#include "TCPServer.h"
#include "nlohmann-json/json.hpp"
#include <fstream>
#include <iostream>

using tcp_demo::ErrorCode;
using tcp_demo::tcp;

class TCPServer::Session : public std::enable_shared_from_this<Session> {
public:
    Session(boost::asio::io_context& io, std::chrono::milliseconds timeout,
            std::function<void(std::shared_ptr<Session>)> on_close)
        : connection_(std::make_shared<tcp_demo::FramedConnection>(io, timeout)),
          on_close_(std::move(on_close)) {}

    tcp::socket& socket() { return connection_->socket(); }

    void start() {
        ErrorCode ec;
        socket().set_option(tcp::no_delay(true), ec);
        if (ec) return stop();
        read();
    }

    void stop() {
        if (stopped_) return;
        stopped_ = true;
        connection_->close();
        on_close_(shared_from_this());
    }

private:
    void read() {
        auto self = shared_from_this();
        connection_->asyncRead([self](ErrorCode ec, std::string message) {
            if (self->stopped_) return;
            if (ec) return self->fail(ec);
            self->connection_->asyncWrite(message, [self](ErrorCode write_ec) {
                if (self->stopped_) return;
                if (write_ec) return self->fail(write_ec);
                self->read();
            });
        });
    }

    void fail(ErrorCode ec) {
        if (ec != boost::asio::error::eof && ec != boost::asio::error::operation_aborted) {
            std::cerr << "Session closed: " << ec.message() << '\n';
        }
        stop();
    }

    std::shared_ptr<tcp_demo::FramedConnection> connection_;
    std::function<void(std::shared_ptr<Session>)> on_close_;
    bool stopped_ = false;
};

TCPServer::TCPServer(boost::asio::io_context& io, std::uint16_t port,
                     const std::string& config_file)
    : io_(io), acceptor_(io), accept_retry_(io), port_(port) {
    if (!config_file.empty()) loadConfiguration(config_file);
}

TCPServer::~TCPServer() { stopServer(); }

void TCPServer::start() {
    if (running_) return;
    acceptor_.open(tcp::v4());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(tcp::endpoint(tcp::v4(), port_));
    acceptor_.listen();
    port_ = acceptor_.local_endpoint().port();
    running_ = true;
    ++generation_;
    std::cout << "Listening on 0.0.0.0:" << port_
              << " (max_connections=" << max_connections_
              << ", timeout_ms=" << timeout_.count() << ")" << std::endl;
    acceptConnection();
}

void TCPServer::acceptConnection() {
    if (!running_) return;
    const auto generation = generation_;
    auto session = std::make_shared<Session>(io_, timeout_,
        [this](std::shared_ptr<Session> closed) { sessions_.erase(closed); });
    acceptor_.async_accept(session->socket(), [this, session, generation](ErrorCode ec) {
        if (!running_ || generation != generation_) return;
        if (ec) {
            std::cerr << "Accept failed: " << ec.message() << "; retrying in 1 s\n";
            accept_retry_.expires_after(std::chrono::seconds(1));
            accept_retry_.async_wait([this, generation](ErrorCode timer_ec) {
                if (!timer_ec && running_ && generation == generation_) acceptConnection();
            });
            return;
        }
        if (sessions_.size() >= max_connections_) {
            session->stop();
        } else {
            sessions_.insert(session);
            session->start();
        }
        acceptConnection();
    });
}

void TCPServer::stopServer() {
    running_ = false;
    ++generation_;
    ErrorCode ignored;
    accept_retry_.cancel(ignored);
    acceptor_.close(ignored);
    while (!sessions_.empty()) {
        auto session = *sessions_.begin();
        session->stop();
    }
}

void TCPServer::restartServer() {
    stopServer();
    start();
}

void TCPServer::loadConfiguration(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open configuration: " + path);
    const auto config = nlohmann::json::parse(file);
    if (!config.is_object()) throw std::runtime_error("Configuration must be a JSON object");
    const auto integer = [&config](const char* key, std::int64_t fallback,
                                   std::int64_t maximum) {
        if (!config.contains(key)) return fallback;
        const auto& value = config.at(key);
        if (!value.is_number_integer() || value < 1 || value > maximum) {
            throw std::runtime_error(std::string(key) + " must be an integer in [1, " +
                                     std::to_string(maximum) + "]");
        }
        return value.get<std::int64_t>();
    };
    max_connections_ = static_cast<std::size_t>(integer("maxConnectionLimit", 5, 10000));
    timeout_ = std::chrono::milliseconds(integer("timeoutDuration", 5000, 3600000));
}
