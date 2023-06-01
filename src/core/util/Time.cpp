//
// Created by Kelan on 01/06/2023.
//

#include "Time.h"

thread_local Time::moment_t Time::s_lastTime = Time::now();


Time::duration_t Time::mark() {
    return mark(s_lastTime);
}

Time::duration_t Time::mark(moment_t startTime) {
    duration_t elapsed = now() - startTime;
    s_lastTime = now();
    return elapsed;
}

double Time::markMsec() {
    return milliseconds(mark());
}

Time::moment_t Time::now() {
    return std::chrono::high_resolution_clock::now();
}

uint64_t Time::nanoseconds(const duration_t& duration) {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

uint64_t Time::nanoseconds(const moment_t& startTime, const moment_t& endTime) {
    return nanoseconds(endTime - startTime);
}

uint64_t Time::nanoseconds(const moment_t& startTime) {
    return nanoseconds(now() - startTime);
}

double Time::milliseconds(const duration_t& duration) {
    return (double)nanoseconds(duration) / 1000000.0;
}

double Time::milliseconds(const moment_t& startTime, const moment_t& endTime) {
    return milliseconds(endTime - startTime);
}

double Time::milliseconds(const moment_t& startTime) {
    return milliseconds(now() - startTime);
}