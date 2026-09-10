#include "TCPClient.h"
#include <boost/asio/connect.hpp>
#include <iostream>

using tcp_demo::ErrorCode;
using tcp_demo::tcp;

TCPClient::TCPClient(boost::asio::io_context& io, ClientOptions options)
    : io_(io), resolver_(io), connect_deadline_(io), schedule_(io), options_(options) {
    if (options_.payload_size > tcp_demo::max_message_size || options_.timeout.count() <= 0 ||
        options_.interval.count() < 0) throw std::invalid_argument("Invalid client options");
}

TCPClient::~TCPClient() {
    on_finished = {};
    stop();
}

void TCPClient::connect(const std::string& host, std::uint16_t port) {
    if (running_) throw std::logic_error("Cannot change endpoint while running");
    if (host.empty() || port == 0) throw std::invalid_argument("Host and nonzero port required");
    host_ = host;
    port_ = std::to_string(port);
}

void TCPClient::startCommunication() {
    if (started_once_) throw std::logic_error("Use a new TCPClient for a new run");
    started_once_ = true;
    running_ = true;
    started_ = Clock::now();
    beginConnection();
}

void TCPClient::beginConnection() {
    if (!running_) return;
    const auto attempt = ++attempt_;
    connection_ = std::make_shared<tcp_demo::FramedConnection>(io_, options_.timeout);
    connect_deadline_.expires_after(options_.timeout);
    connect_deadline_.async_wait([this, attempt](ErrorCode ec) {
        if (!ec && current(attempt)) retry(boost::asio::error::timed_out);
    });
    resolver_.async_resolve(host_, port_,
        [this, attempt](ErrorCode ec, tcp::resolver::results_type endpoints) {
            if (!current(attempt)) return;
            if (ec) return retry(ec);
            auto connection = connection_;
            boost::asio::async_connect(connection->socket(), endpoints,
                [this, attempt, connection](ErrorCode connect_ec, const tcp::endpoint&) {
                    if (!current(attempt)) return;
                    if (connect_ec) return retry(connect_ec);
                    ErrorCode ignored;
                    connect_deadline_.cancel(ignored);
                    // Invalidate any successful timer completion already queued.
                    ++attempt_;
                    ErrorCode option_ec;
                    connection->socket().set_option(tcp::no_delay(true), option_ec);
                    if (option_ec) return retry(option_ec);
                    ++connections_;
                    std::cerr << "Connected to " << host_ << ':' << port_ << '\n';
                    exchangeMessage();
                });
        });
}

void TCPClient::exchangeMessage() {
    if (!running_) return;
    const auto attempt = attempt_;
    message_.assign(options_.payload_size, 'x');
    const auto label = "message-" + std::to_string(messages_ + 1) + " ";
    message_.replace(0, std::min(label.size(), message_.size()),
                     label.substr(0, message_.size()));
    request_started_ = Clock::now();
    connection_->asyncWrite(message_, [this, attempt](ErrorCode ec) {
        if (!current(attempt)) return;
        if (ec) return retry(ec);
        connection_->asyncRead([this, attempt](ErrorCode read_ec, std::string reply) {
            if (!current(attempt)) return;
            if (read_ec) return retry(read_ec);
            if (reply != message_) return retry(
                boost::system::errc::make_error_code(boost::system::errc::protocol_error));
            latencies_.add(std::chrono::duration<double, std::milli>(
                Clock::now() - request_started_).count());
            ++messages_;
            retry_delay_ = std::chrono::milliseconds(100);
            if (options_.message_count != 0 && messages_ >= options_.message_count) {
                stop();
                return;
            }
            schedule_.expires_after(options_.interval);
            schedule_.async_wait([this, attempt](ErrorCode timer_ec) {
                if (!timer_ec && current(attempt)) exchangeMessage();
            });
        });
    });
}

void TCPClient::retry(ErrorCode ec) {
    if (!running_) return;
    ++failures_;
    ++attempt_;
    resolver_.cancel();
    ErrorCode ignored;
    connect_deadline_.cancel(ignored);
    if (connection_) connection_->close();
    std::cerr << "Connection/exchange failed: " << ec.message()
              << "; retrying in " << retry_delay_.count() << " ms\n";
    const auto attempt = attempt_;
    schedule_.expires_after(retry_delay_);
    retry_delay_ = std::min(retry_delay_ * 2, std::chrono::milliseconds(2000));
    schedule_.async_wait([this, attempt](ErrorCode timer_ec) {
        if (!timer_ec && current(attempt)) beginConnection();
    });
}

void TCPClient::stop() {
    if (!running_) return;
    running_ = false;
    finished_ = Clock::now();
    ++attempt_;
    resolver_.cancel();
    ErrorCode ignored;
    connect_deadline_.cancel(ignored);
    schedule_.cancel(ignored);
    if (connection_) connection_->close();
    if (on_finished) on_finished();
}

nlohmann::json TCPClient::statistics() const {
    const auto end = running_ ? Clock::now() : finished_;
    const double elapsed = started_once_ ? std::chrono::duration<double>(end - started_).count() : 0;
    return {
        {"messages", messages_}, {"payload_bytes", options_.payload_size},
        {"connections", connections_}, {"reconnects", connections_ > 0 ? connections_ - 1 : 0},
        {"failures", failures_}, {"elapsed_seconds", elapsed},
        {"messages_per_second", elapsed > 0 ? messages_ / elapsed : 0},
        {"latency_ms", {{"sample_count", latencies_.size()}, {"mean", latencies_.mean()},
                        {"min", latencies_.percentile(0)}, {"p50", latencies_.percentile(.50)},
                        {"p95", latencies_.percentile(.95)}, {"p99", latencies_.percentile(.99)},
                        {"max", latencies_.percentile(1)}}}
    };
}
