
#include "Profiler.h"

thread_local Performance::moment_t Performance::s_lastTime = Performance::now();

Performance::duration_t Performance::mark() {
    return mark(s_lastTime);
}

Performance::duration_t Performance::mark(moment_t startTime) {
    duration_t elapsed = now() - startTime;
    s_lastTime = now();
    return elapsed;
}

double Performance::markMsec() {
    return milliseconds(mark());
}

Performance::moment_t Performance::now() {
    return std::chrono::high_resolution_clock::now();
}

uint64_t Performance::nanoseconds(const duration_t& duration) {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

uint64_t Performance::nanoseconds(const moment_t& startTime, const moment_t& endTime) {
    return nanoseconds(endTime - startTime);
}

uint64_t Performance::nanoseconds(const moment_t& startTime) {
    return nanoseconds(now() - startTime);
}

double Performance::milliseconds(const duration_t& duration) {
    return nanoseconds(duration) / 1000000.0;
}

double Performance::milliseconds(const moment_t& startTime, const moment_t& endTime) {
    return milliseconds(endTime - startTime);
}

double Performance::milliseconds(const moment_t& startTime) {
    return milliseconds(now() - startTime);
}

Profiler::Profiler() {

}

Profiler::~Profiler() {

}

profile_id Profiler::id(const char* name) {
    return __itt_string_handle_createA(name);
}

__itt_domain* Profiler::domain() {
    static __itt_domain* s_domain = __itt_domain_createA(PROFILE_DOMAIN_NAME);
    return s_domain;
}

void Profiler::begin(profile_id const& id) {
    __itt_task_begin(domain(), __itt_null, __itt_null, id);
}

void Profiler::end() {
    __itt_task_end(domain());
}

ScopeProfiler::ScopeProfiler(profile_id const& id):
    m_currentRegionId(nullptr) {
    Profiler::begin(id);
}

ScopeProfiler::~ScopeProfiler() {
    endRegion();
    Profiler::end();
}

void ScopeProfiler::beginRegion(const profile_id& id) {
    endRegion();
    Profiler::begin(id);
    m_currentRegionId = id;
}

void ScopeProfiler::endRegion() {
    if (m_currentRegionId != nullptr) {
        m_currentRegionId = nullptr;
        Profiler::end();
    }
}
