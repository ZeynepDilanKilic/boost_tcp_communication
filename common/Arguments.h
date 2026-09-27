#pragma once
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tcp_demo {
inline std::uint64_t unsignedArgument(std::string_view text, std::uint64_t maximum,
                                      const char* name) {
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value > maximum) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + std::string(text));
    }
    return value;
}
} // namespace tcp_demo
