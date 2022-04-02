
#ifndef WORLDENGINE_PROFILER_H
#define WORLDENGINE_PROFILER_H

#include "core/core.h"
#include <chrono>
#include <iostream>
#include <ittnotify.h>

#ifndef PROFILE_DOMAIN_NAME
#define PROFILE_DOMAIN_NAME "WorldEngine"
#endif

typedef __itt_string_handle* profile_id;

namespace Performance {
    using duration_t = std::chrono::nanoseconds;
    using moment_t = std::chrono::high_resolution_clock::time_point;

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


class Profiler {
public:
    Profiler();

    ~Profiler();

    static profile_id id(const char* name);

    static __itt_domain* domain();

    static void begin(const profile_id& id);

    static void end();
};

class ScopeProfiler {
public:
    ScopeProfiler(const profile_id& id);

    ~ScopeProfiler();

    void beginRegion(const profile_id& id);

    void endRegion();

private:
    profile_id m_currentRegionId;
};


#define CONCAT1(a, b) a##b
#define CONCAT(a,b) CONCAT1(a,b) // Why two???? :(
#define PFID_NAME(name) CONCAT(name, __LINE__)
#define PF_NAME __current_scope_profiler
#define PROFILE_SCOPE(name) \
    static profile_id PFID_NAME(__pf_scp_id_) = Profiler::id(name); \
    ScopeProfiler PF_NAME(PFID_NAME(__pf_scp_id_));

#define PROFILE_REGION(name) {\
    static profile_id PFID_NAME(__pf_reg_id_) = Profiler::id(name); \
    PF_NAME.beginRegion(PFID_NAME(__pf_reg_id_)); }

#define PROFILE_END_REGION(x) { \
    PF_NAME.endRegion(); }


#endif //WORLDENGINE_PROFILER_H
