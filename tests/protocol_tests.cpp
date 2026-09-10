#include "common/Arguments.h"
#include "common/LatencyStats.h"
#include "common/Protocol.h"
#include <iostream>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        check(tcp_demo::encodeLength(0x010203) == tcp_demo::Header{0, 1, 2, 3},
              "Length must use four bytes in network byte order");
        check(tcp_demo::decodeLength({0, 0, 1, 0}) == 256, "Decode an independent wire fixture");
        for (auto length : {0u, 1u, 1024u, tcp_demo::max_message_size}) {
            check(tcp_demo::decodeLength(tcp_demo::encodeLength(length)) == length,
                  "Boundary lengths must round-trip");
        }
        bool rejected = false;
        try { (void)tcp_demo::decodeLength({255, 255, 255, 255}); }
        catch (const std::length_error&) { rejected = true; }
        check(rejected, "Reject oversized wire length before allocating a body");
        rejected = false;
        try { (void)tcp_demo::encodeLength(tcp_demo::max_message_size + 1); }
        catch (const std::length_error&) { rejected = true; }
        check(rejected, "Reject oversized outgoing length");

        tcp_demo::LatencyStats stats;
        check(stats.mean() == 0 && stats.percentile(.95) == 0, "Empty statistics");
        for (int value = 100; value > 0; --value) stats.add(value);
        check(stats.percentile(.50) == 50 && stats.percentile(.95) == 95 &&
              stats.percentile(.99) == 99 && stats.mean() == 50.5, "Known nearest-rank percentiles");
        for (std::size_t i = 0; i < tcp_demo::LatencyStats::capacity; ++i) stats.add(7);
        check(stats.size() == tcp_demo::LatencyStats::capacity && stats.mean() == 7,
              "Continuous runs must retain a bounded window");
        for (const auto* bad : {"-1", "123x", "", "65536", "18446744073709551616"}) {
            rejected = false;
            try { (void)tcp_demo::unsignedArgument(bad, 65535, "port"); }
            catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "Invalid numeric arguments must not wrap or partially parse");
        }
        std::cout << "Protocol boundaries, argument validation and statistics passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
