#pragma once
#include "Protocol.h"
#include <utility>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tcp_demo {
using ErrorCode = boost::system::error_code;
using tcp = boost::asio::ip::tcp;

// One read OR write at a time, on one io_context thread. The demos deliberately
// use request/response flow control, with at most one message in flight.
class FramedConnection : public std::enable_shared_from_this<FramedConnection> {
public:
    using ReadHandler = std::function<void(ErrorCode, std::string)>;
    using WriteHandler = std::function<void(ErrorCode)>;

    FramedConnection(boost::asio::io_context& io, std::chrono::milliseconds timeout)
        : socket_(io), timer_(io), timeout_(timeout) {}
    tcp::socket& socket() { return socket_; }

    void close() {
        ++operation_;
        ErrorCode ignored;
        timer_.cancel(ignored);
        socket_.close(ignored);
    }

    void asyncRead(ReadHandler handler) {
        armDeadline();
        auto self = shared_from_this();
        boost::asio::async_read(socket_, boost::asio::buffer(header_),
            [self, handler = std::move(handler)](ErrorCode ec, std::size_t) mutable {
                if (ec) {
                    handler(self->finish(ec), {});
                    return;
                }
                std::uint32_t length;
                try {
                    length = decodeLength(self->header_);
                } catch (const std::length_error&) {
                    handler(self->finish(boost::asio::error::message_size), {});
                    return;
                }
                self->incoming_.resize(length);
                if (length == 0) {
                    handler(self->finish({}), {});
                    return;
                }
                boost::asio::async_read(self->socket_, boost::asio::buffer(self->incoming_),
                    [self, handler = std::move(handler)](ErrorCode body_ec, std::size_t) {
                        handler(self->finish(body_ec),
                                body_ec ? std::string{} : std::move(self->incoming_));
                    });
            });
    }

    void asyncWrite(const std::string& message, WriteHandler handler) {
        if (message.size() > max_message_size) throw std::length_error("Message exceeds the 1 MiB limit");
        const auto header = encodeLength(static_cast<std::uint32_t>(message.size()));
        outgoing_.assign(header.begin(), header.end());
        outgoing_.insert(outgoing_.end(), message.begin(), message.end());
        armDeadline();
        auto self = shared_from_this();
        boost::asio::async_write(socket_, boost::asio::buffer(outgoing_),
            [self, handler = std::move(handler)](ErrorCode ec, std::size_t) {
                handler(self->finish(ec));
            });
    }

private:
    void armDeadline() {
        const auto operation = ++operation_;
        timed_out_ = false;
        timer_.expires_after(timeout_);
        auto self = shared_from_this();
        timer_.async_wait([self, operation](ErrorCode ec) {
            // A cancelled timer may already have its completion queued.
            if (!ec && operation == self->operation_) {
                self->timed_out_ = true;
                ErrorCode ignored;
                self->socket_.close(ignored);
            }
        });
    }

    ErrorCode finish(ErrorCode ec) {
        ++operation_;
        ErrorCode ignored;
        timer_.cancel(ignored);
        return timed_out_ ? ErrorCode(boost::asio::error::timed_out) : ec;
    }

    tcp::socket socket_;
    boost::asio::steady_timer timer_;
    std::chrono::milliseconds timeout_;
    Header header_{};
    std::string incoming_;
    std::vector<unsigned char> outgoing_;
    std::uint64_t operation_ = 0;
    bool timed_out_ = false;
};
} // namespace tcp_demo
