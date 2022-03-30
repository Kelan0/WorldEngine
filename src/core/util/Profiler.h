
#ifndef WORLDENGINE_PROFILER_H
#define WORLDENGINE_PROFILER_H

#include "core/core.h"
#include <chrono>
#include <iostream>

class Profiler {
    NO_COPY(Profiler);
    NO_MOVE(Profiler);
public:
    using duration_t = std::chrono::nanoseconds;
    using moment_t = std::chrono::high_resolution_clock::time_point;

public:
    Profiler();

    ~Profiler();

    duration_t mark();

    duration_t mark(moment_t startTime);

    double markMsec();

    static moment_t now();

    static uint64_t nanoseconds(const duration_t& duration);

    static uint64_t nanoseconds(const moment_t& startTime, const moment_t& endTime);

    static uint64_t nanoseconds(const moment_t& startTime);

    static double milliseconds(const duration_t& duration);

    static double milliseconds(const moment_t& startTime, const moment_t& endTime);

    static double milliseconds(const moment_t& startTime);

private:
    moment_t m_lastTime;
};


#endif //WORLDENGINE_PROFILER_H
