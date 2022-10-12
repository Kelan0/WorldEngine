
#ifndef WORLDENGINE_PROFILER_H
#define WORLDENGINE_PROFILER_H

#include "core/core.h"
#include <chrono>
#include <iostream>
#include "core/graphics/FrameResource.h"
#include "core/engine/scene/event/Events.h"

#if ITT_ENABLED
#include <ittnotify.h>
#endif

#ifndef PROFILE_DOMAIN_NAME
#define PROFILE_DOMAIN_NAME "WorldEngine"
#endif

#define PROFILING_ENABLED 1
#define INTERNAL_PROFILING_ENABLED 1


struct __profile_handle {
    const char* name = nullptr;
#if ITT_ENABLED
    __itt_string_handle* itt_handle = nullptr;
#endif
};
typedef __profile_handle* profile_id;


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
private:
    struct GPUQueryPool {
        vk::QueryPool pool = VK_NULL_HANDLE;
        uint32_t capacity = 0;
        uint32_t size = 0;
    };

    struct GPUQuery {
        uint32_t queryIndex = 0;
        GPUQueryPool* queryPool = nullptr;
    };

    struct GPUQueryFrameData {
        std::vector<GPUQueryPool> queryPools;
        uint32_t queryCount = 0;
    };

public:
    struct Profile {
        profile_id id = nullptr;
        size_t parentIndex = SIZE_MAX;
        size_t lastChildIndex = SIZE_MAX;
        size_t nextSiblingIndex = SIZE_MAX;
    };

    struct CPUProfile : public Profile {
        Performance::moment_t startTime;
        Performance::moment_t endTime;
    };

    struct GPUProfile : public Profile {
        Performance::moment_t startTime;
        Performance::moment_t endTime;
        GPUQuery startQuery;
        GPUQuery endQuery;
    };

    struct ProfileContext {
        bool frameStarted = false;
        size_t currentIndex = SIZE_MAX;
        std::mutex mtx;
    };

    struct ThreadContext : public ProfileContext {
        bool threadActive = false;
        bool hasGpuProfiles = false;
        std::vector<CPUProfile> frameProfiles;
        std::vector<CPUProfile> prevFrameProfiles;

        ThreadContext();

        ~ThreadContext();
    };

    struct GPUContext : public ProfileContext {
        std::vector<GPUProfile> frameProfiles;
        std::vector<GPUProfile> prevFrameProfiles;
        FrameResource<GPUQueryFrameData> queryData;

        GPUContext();

        ~GPUContext();
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

    static void beginCPU(const profile_id& id);

    static void endCPU();

    static void beginGPU(const profile_id& id);

    static void endGPU();

    static void getFrameProfile(std::unordered_map<uint64_t, std::vector<CPUProfile>>& outThreadProfiles);

    static bool isGpuProfilingEnabled();

    static float getGpuProfilingResolutionNanoseconds();

private:
    static bool writeTimestamp(const vk::CommandBuffer& commandBuffer, const vk::PipelineStageFlagBits& pipelineStage, GPUQuery* outQuery);

    static bool getGpuQueryPool(const uint32_t& maxQueries, GPUQueryPool** queryPool);

    static bool createGpuTimestampQueryPool(const uint32_t& capacity, vk::QueryPool* queryPool);

    static void onCleanupGraphics(const ShutdownGraphicsEvent& event);

    static ThreadContext& threadContext();

    static GPUContext& gpuContext();

private:
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    static std::unordered_map<uint64_t, ThreadContext*> s_threadContexts;
    static std::mutex s_threadContextsMtx;
#endif
};






class ScopeProfiler {
public:
    explicit ScopeProfiler(const profile_id& id);

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

#define PROFILE_END_REGION() { \
    PF_NAME.endRegion(); }

#define PROFILE_CATEGORY(name)

#else

#define PROFILE_SCOPE(name)
#define PROFILE_REGION(name)
#define PROFILE_END_REGION(x)

#endif

#endif //WORLDENGINE_PROFILER_H
