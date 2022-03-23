
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

double Profiler::milliseconds(const duration_t& duration) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1000000.0;
}

double Profiler::milliseconds(const moment_t& startTime, const moment_t& endTime) {
    return milliseconds(endTime - startTime);
}

double Profiler::milliseconds(const moment_t& startTime) {
    return milliseconds(now() - startTime);
}
