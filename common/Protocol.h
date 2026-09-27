#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace tcp_demo {
inline constexpr std::uint32_t max_message_size = 1024 * 1024;
using Header = std::array<unsigned char, 4>;

inline Header encodeLength(std::uint32_t length) {
    if (length > max_message_size) throw std::length_error("Message exceeds the 1 MiB limit");
    return {static_cast<unsigned char>(length >> 24), static_cast<unsigned char>(length >> 16),
            static_cast<unsigned char>(length >> 8), static_cast<unsigned char>(length)};
}

inline std::uint32_t decodeLength(const Header& header) {
    const auto length = (std::uint32_t{header[0]} << 24) | (std::uint32_t{header[1]} << 16) |
                        (std::uint32_t{header[2]} << 8) | std::uint32_t{header[3]};
    if (length > max_message_size) throw std::length_error("Message exceeds the 1 MiB limit");
    return length;
}
} // namespace tcp_demo
