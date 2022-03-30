
#include "Profiler.h"

Profiler::Profiler():
    m_lastTime(now()) {

}

Profiler::~Profiler() {

}

Profiler::duration_t Profiler::mark() {
    return mark(m_lastTime);
}

Profiler::duration_t Profiler::mark(moment_t startTime) {
    duration_t elapsed = now() - startTime;
    m_lastTime = now();
    return elapsed;
}

double Profiler::markMsec() {
    return milliseconds(mark());
}

Profiler::moment_t Profiler::now() {
    return std::chrono::high_resolution_clock::now();
}

uint64_t Profiler::nanoseconds(const duration_t& duration) {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

uint64_t Profiler::nanoseconds(const moment_t& startTime, const moment_t& endTime) {
    return nanoseconds(endTime - startTime);
}

uint64_t Profiler::nanoseconds(const moment_t& startTime) {
    return nanoseconds(now() - startTime);
}

double Profiler::milliseconds(const duration_t& duration) {
    return nanoseconds(duration) / 1000000.0;
}

double Profiler::milliseconds(const moment_t& startTime, const moment_t& endTime) {
    return milliseconds(endTime - startTime);
}

double Profiler::milliseconds(const moment_t& startTime) {
    return milliseconds(now() - startTime);
}
