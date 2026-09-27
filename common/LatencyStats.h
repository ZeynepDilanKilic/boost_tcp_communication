#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <numeric>
#include <vector>

namespace tcp_demo {
class LatencyStats {
public:
    static constexpr std::size_t capacity = 10000;
    void add(double milliseconds) {
        if (samples_.size() == capacity) samples_.pop_front();
        samples_.push_back(milliseconds);
    }
    std::size_t size() const { return samples_.size(); }
    double mean() const {
        return samples_.empty() ? 0.0 :
            std::accumulate(samples_.begin(), samples_.end(), 0.0) / samples_.size();
    }
    // Nearest-rank percentile over the latest capacity successful exchanges.
    double percentile(double fraction) const {
        if (samples_.empty()) return 0.0;
        std::vector<double> sorted(samples_.begin(), samples_.end());
        std::sort(sorted.begin(), sorted.end());
        const auto rank = static_cast<std::size_t>(std::ceil(
            std::clamp(fraction, 0.0, 1.0) * sorted.size()));
        return sorted[rank == 0 ? 0 : rank - 1];
    }
private:
    std::deque<double> samples_;
};
} // namespace tcp_demo
