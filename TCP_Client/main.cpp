#include "TCPClient.h"
#include "common/Arguments.h"
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <iostream>
#include <limits>

int main(int argc, char* argv[]) {
    if (argc > 7 || (argc > 1 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: TCP_Client [host=127.0.0.1] [port=12345] [count=10] "
                     "[interval_ms=1000] [payload_bytes=64] [timeout_ms=3000]\n"
                     "count=0 runs until Ctrl+C. JSON statistics go to stdout.\n";
        return argc > 7 ? 1 : 0;
    }
    try {
        const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
        const auto port = argc > 2 ? tcp_demo::unsignedArgument(argv[2], 65535, "port") : 12345;
        ClientOptions options;
        if (argc > 3) options.message_count = tcp_demo::unsignedArgument(
            argv[3], std::numeric_limits<std::uint64_t>::max(), "count");
        if (argc > 4) options.interval = std::chrono::milliseconds(
            tcp_demo::unsignedArgument(argv[4], 3600000, "interval_ms"));
        if (argc > 5) options.payload_size = static_cast<std::size_t>(
            tcp_demo::unsignedArgument(argv[5], tcp_demo::max_message_size, "payload_bytes"));
        if (argc > 6) options.timeout = std::chrono::milliseconds(
            tcp_demo::unsignedArgument(argv[6], 3600000, "timeout_ms"));
        boost::asio::io_context io;
        TCPClient client(io, options);
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        client.on_finished = [&signals] { signals.cancel(); };
        signals.async_wait([&client](tcp_demo::ErrorCode ec, int) {
            if (!ec) client.stop();
        });
        client.connect(host, static_cast<std::uint16_t>(port));
        client.startCommunication();
        io.run();
        std::cout << client.statistics().dump(2) << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Client error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
