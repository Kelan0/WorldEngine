
#ifndef WORLDENGINE_PROFILER_H
#define WORLDENGINE_PROFILER_H

#include "core/core.h"
#include <chrono>
#include <iostream>

#if ITT_ENABLED
#include <ittnotify.h>
#endif

#ifndef PROFILE_DOMAIN_NAME
#define PROFILE_DOMAIN_NAME "WorldEngine"
#endif

#define PROFILING_ENABLED 1
#define INTERNAL_PROFILING_ENABLED 1


struct _profile_handle {
    const char* name = nullptr;
#if ITT_ENABLED
    __itt_string_handle* itt_handle = nullptr;
#endif
};
typedef _profile_handle* profile_id;


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
    struct Profile {
        profile_id id = nullptr;
        Performance::moment_t startCPU;
        Performance::moment_t endCPU;
        size_t parentIndex = SIZE_MAX;
        size_t lastChildIndex = SIZE_MAX;
        size_t nextSiblingIndex = SIZE_MAX;
    };

    struct ThreadContext {
        bool threadActive = false;
        bool frameStarted = false;
        size_t currentIndex;
        std::vector<Profile> frameProfiles;
        std::vector<Profile> prevFrameProfiles;
        std::mutex mtx;

        ThreadContext();

        ~ThreadContext();
    };

private:
    Profiler();

    ~Profiler();

public:
    static profile_id id(const char* name);

#if ITT_ENABLED
    static __itt_domain* domain();
#endif

    static void beginFrame();

    static void endFrame();

    static void begin(const profile_id& id);

    static void end();

    static void getFrameProfile(std::unordered_map<uint64_t, std::vector<Profile>>& outThreadProfiles);

private:
    void flushFrames();

private:
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    static thread_local ThreadContext s_context;
    static std::unordered_map<uint64_t, ThreadContext*> s_threadContexts;
    static std::mutex s_threadContextsMtx;
#endif
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


#if PROFILING_ENABLED
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

#define PROFILE_CATEGORY(name)

#else

#define PROFILE_SCOPE(name)
#define PROFILE_REGION(name)
#define PROFILE_END_REGION(x)

#endif

#endif //WORLDENGINE_PROFILER_H
