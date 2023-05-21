
#ifndef WORLDENGINE_PROFILER_H
#define WORLDENGINE_PROFILER_H

#include "core/core.h"
#include <chrono>
#include <iostream>
#include "core/graphics/FrameResource.h"

#if ITT_ENABLED
#include <ittnotify.h>
#endif

#ifndef PROFILE_DOMAIN_NAME
#define PROFILE_DOMAIN_NAME "WorldEngine"
#endif

#define PROFILING_ENABLED 1
#define INTERNAL_PROFILING_ENABLED 1

#ifndef PROFILE_GPU_STACK_LIMIT
#define PROFILE_GPU_STACK_LIMIT 128
#endif

#ifndef PROFILE_CPU_STACK_LIMIT
#define PROFILE_CPU_STACK_LIMIT 512
#endif

struct ShutdownGraphicsEvent;
struct RecreateSwapchainEvent;

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


class Profiler {
private:
    struct GPUQueryPool {
        WeakResource<vkr::Device> device;
        vk::QueryPool pool = nullptr;
        uint32_t capacity = 0;
        uint32_t size = 0;
        bool allAvailable = false;
        std::vector<uint64_t> queryResults;
        uint32_t id = 0;
        std::string name;
        uint32_t framesSinceDestroyed = 0;
    };

    struct GPUQuery {
        uint32_t queryIndex = UINT32_MAX;
        GPUQueryPool* queryPool = nullptr;
        double time = 0.0;
#if _DEBUG
      bool queryWritten = false;
      bool queryReceived = false;
      bool failedToGetQueryPool = false;
#endif
    };

    struct GPUQueryFrameData {

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
        std::vector<GPUProfile> allFrameProfiles;
        std::vector<size_t> allFrameStartIndexOffsets;
        size_t latestReadyFrameIndex = SIZE_MAX;
        std::vector<GPUQueryPool*> queryPools;
        std::vector<GPUQueryPool*> unusedQueryPools;
        std::vector<GPUQueryPool*> destroyedQueryPools;
        size_t currentQueryPoolIndex = SIZE_MAX;
        uint32_t minQueryPoolSize = 25;
        int32_t profileStackDepth = 0;
#if _DEBUG
        std::unordered_map<std::string, int32_t> debugOpenProfiles;
#endif

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

    static void beginGraphicsFrame();

    static void endGraphicsFrame();

    static void beginGPU(const profile_id& id, const vk::CommandBuffer& commandBuffer);

    static void endGPU(const std::string& profileName, const vk::CommandBuffer& commandBuffer);

    static void getFrameProfile(std::unordered_map<uint64_t, std::vector<CPUProfile>>& outThreadProfiles);

    static bool getLatestGpuFrameProfile(std::vector<GPUProfile>& outGpuProfiles);

    static bool isGpuProfilingEnabled();

    static float getGpuProfilingResolutionNanoseconds();

private:
    static bool writeTimestamp(const vk::CommandBuffer& commandBuffer, const vk::PipelineStageFlagBits& pipelineStage, GPUQuery* outQuery);

    static bool getNextQueryPool(const vk::CommandBuffer& commandBuffer, GPUQueryPool** queryPool);

    static bool createGpuTimestampQueryPool(const vk::Device& device, uint32_t capacity, vk::QueryPool* queryPool);

    static void destroyQueryPool(GPUQueryPool* queryPool);

    static void resetQueryPools(const vk::CommandBuffer& commandBuffer, GPUQueryPool** queryPools, size_t count);

    static void onCleanupGraphics(ShutdownGraphicsEvent* event);

    static void onRecreateSwapchain(RecreateSwapchainEvent* event);

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

#define PROFILE_BEGIN_GPU_CMD(name, commandBuffer) {\
    static profile_id PFID_NAME(__pf_gpu_cmd_id_) = Profiler::id(name); \
    Profiler::beginGPU(PFID_NAME(__pf_gpu_cmd_id_), commandBuffer); \
}

#define PROFILE_END_GPU_CMD(name, commandBuffer) {\
    Profiler::endGPU(name, commandBuffer); \
}

#define PROFILE_CATEGORY(name)

#else

#define PROFILE_SCOPE(name)
#define PROFILE_REGION(name)
#define PROFILE_END_REGION(x)

#endif

#endif //WORLDENGINE_PROFILER_H
