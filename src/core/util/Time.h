

#ifndef WORLDENGINE_TIME_H
#define WORLDENGINE_TIME_H

#include <chrono>

namespace Time {
    using duration_t = std::chrono::nanoseconds;
    using moment_t = std::chrono::high_resolution_clock::time_point;
    constexpr moment_t zero_moment = moment_t{};
    constexpr duration_t zero_duration = duration_t{};

    duration_t mark();

    duration_t mark(moment_t startTime);

    double markMsec();

    moment_t now();

    uint64_t nanoseconds(const duration_t& duration);

    uint64_t nanoseconds(const moment_t& startTime, const moment_t& endTime);

    uint64_t nanoseconds(const moment_t& startTime);

    double milliseconds(const duration_t& duration);

    double milliseconds(const moment_t& startTime, const moment_t& endTime);

    double milliseconds(const moment_t& startTime);

    extern thread_local moment_t s_lastTime;
};

#endif //WORLDENGINE_TIME_H
