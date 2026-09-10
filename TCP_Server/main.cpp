#include "TCPServer.h"
#include "common/Arguments.h"
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc > 3 || (argc > 1 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: TCP_Server [port=12345] [config.json]\n";
        return argc > 3 ? 1 : 0;
    }
    try {
        const auto port = argc > 1 ? tcp_demo::unsignedArgument(argv[1], 65535, "port") : 12345;
        boost::asio::io_context io;
        TCPServer server(io, static_cast<std::uint16_t>(port), argc > 2 ? argv[2] : "");
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&server](tcp_demo::ErrorCode ec, int) {
            if (!ec) server.stopServer();
        });
        server.start();
        io.run();
    } catch (const std::exception& error) {
        std::cerr << "Server error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
