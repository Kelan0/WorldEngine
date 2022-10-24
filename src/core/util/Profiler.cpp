
#include "Profiler.h"
#include "core/thread/ThreadUtils.h"
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include <thread>

thread_local Performance::moment_t Performance::s_lastTime = Performance::now();
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
std::unordered_map<uint64_t, Profiler::ThreadContext*> Profiler::s_threadContexts;
std::mutex Profiler::s_threadContextsMtx;
#endif

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
    return (double)nanoseconds(duration) / 1000000.0;
}

double Performance::milliseconds(const moment_t& startTime, const moment_t& endTime) {
    return milliseconds(endTime - startTime);
}

double Performance::milliseconds(const moment_t& startTime) {
    return milliseconds(now() - startTime);
}




Profiler::ThreadContext::ThreadContext() {
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    uint64_t currentId = ThreadUtils::getCurrentThreadHashedId();

    std::scoped_lock<std::mutex> lock(s_threadContextsMtx);
    s_threadContexts.insert(std::make_pair(currentId, this));

#endif
}

Profiler::ThreadContext::~ThreadContext() {
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    uint64_t currentId = ThreadUtils::getCurrentThreadHashedId();

    if (Application::instance() != nullptr) {
        std::scoped_lock<std::mutex> lock(s_threadContextsMtx);
        s_threadContexts.erase(currentId); // TODO: why does this cause an exception when closing the application?
    }

#endif
}

Profiler::GPUContext::GPUContext() {
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    printf("Creating GPUContext on thread (0x%016llx)\n", ThreadUtils::getCurrentThreadHashedId());
    Engine::eventDispatcher()->connect(&Profiler::onCleanupGraphics);
#endif
}

Profiler::GPUContext::~GPUContext() = default;

Profiler::Profiler() = default;

Profiler::~Profiler() = default;

profile_id Profiler::id(const char* name) {
    static std::unordered_map<std::string, __profile_handle*> s_allHandles;
    auto it = s_allHandles.find(name);
    if (it == s_allHandles.end()) {
        it = s_allHandles.insert(std::make_pair(std::string(name), new __profile_handle{})).first;
        it->second->name = it->first.c_str();
#if ITT_ENABLED
        it->second->itt_handle = __itt_string_handle_createA(it->second->name);
#endif
    }
    return it->second;
}

#if ITT_ENABLED
__itt_domain* Profiler::domain() {
    static __itt_domain* s_domain = __itt_domain_createA(PROFILE_DOMAIN_NAME);
    return s_domain;
}
#endif

void Profiler::beginFrame() {
#if PROFILING_ENABLED
//    printf("======== FRAME BEGIN thread 0x%016llx\n", ThreadUtils::getCurrentThreadHashedId());

#if INTERNAL_PROFILING_ENABLED
    ThreadContext& ctx = threadContext();
    ctx.threadActive = true;

    ctx.currentIndex = SIZE_MAX;
    ctx.frameProfiles.clear();

    ctx.frameStarted = true;
#endif

    static profile_id id = Profiler::id("Frame");
    Profiler::beginCPU(id);
#endif
}

void Profiler::endFrame() {
#if PROFILING_ENABLED
    Profiler::endCPU();

#if INTERNAL_PROFILING_ENABLED
    ThreadContext& ctx = threadContext();
    ctx.frameStarted = false;

    {
        std::scoped_lock<std::mutex> lock(ctx.mtx);
        ctx.prevFrameProfiles.swap(ctx.frameProfiles);
    }
#endif
#endif
}

void Profiler::beginCPU(const profile_id& id) {
#if PROFILING_ENABLED
#if ITT_ENABLED
    __itt_task_begin(domain(), __itt_null, __itt_null, id->itt_handle);
#endif

#if INTERNAL_PROFILING_ENABLED
    ThreadContext& ctx = threadContext();
    if (!ctx.frameStarted)
        return; // Ignore constructing frame profiles if this is not part of a frame (e.g. during initialization).

    size_t parentIndex = ctx.currentIndex;
    ctx.currentIndex = ctx.frameProfiles.size();

    CPUProfile* profile = &ctx.frameProfiles.emplace_back(CPUProfile{});
    profile->id = id;
    profile->parentIndex = parentIndex;

    if (parentIndex != SIZE_MAX) {
        CPUProfile& parent = ctx.frameProfiles[parentIndex];
        if (parent.lastChildIndex != SIZE_MAX) {
            CPUProfile& lastChild = ctx.frameProfiles[parent.lastChildIndex];
            lastChild.nextSiblingIndex = ctx.currentIndex;
        }
        parent.lastChildIndex = ctx.currentIndex;
    }

    profile->startTime = Performance::now();
#endif
#endif
}

void Profiler::endCPU() {
#if PROFILING_ENABLED
#if ITT_ENABLED
    __itt_task_end(domain());
#endif

#if INTERNAL_PROFILING_ENABLED
    ThreadContext& ctx = threadContext();
    if (!ctx.frameStarted)
        return;

    assert(ctx.currentIndex < ctx.frameProfiles.size());
    CPUProfile* profile = &ctx.frameProfiles[ctx.currentIndex];
    ctx.currentIndex = profile->parentIndex;
    profile->endTime = Performance::now();
#endif
#endif
}

void Profiler::beginGraphicsFrame(const vk::CommandBuffer& commandBuffer) {
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();
    ctx.currentIndex = SIZE_MAX;
//    ctx.frameProfiles.clear(); // TODO: Remove oldest frame profiles that have a query response.
    ctx.allFrameStartIndexOffsets.emplace_back(ctx.allFrameProfiles.size());
    ctx.frameStarted = true;
#endif
#endif
}

void Profiler::endGraphicsFrame(const vk::CommandBuffer& commandBuffer) {
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();

    const vk::Device& device = **Engine::graphics()->getDevice();

    vk::Result result;

    ctx.currentQueryPool = nullptr;

    vk::QueryResultFlags flags = vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability;

    size_t dataSize;
    uint32_t strideBytes = sizeof(uint64_t) * 2;

    for (auto& queryPool : ctx.queryPools) {
        if (queryPool->available < queryPool->size) {
            queryPool->queryResults.resize(queryPool->size * 2);
            dataSize = queryPool->queryResults.size() * sizeof(uint64_t);
            result = device.getQueryPoolResults(queryPool->pool, 0, queryPool->size, dataSize, queryPool->queryResults.data(), (vk::DeviceSize)strideBytes, flags);

            queryPool->available = 0;
            for (uint32_t i = 0; i < queryPool->size; ++i) {
                if (queryPool->queryResults[i * 2] != 0) {
                    ++queryPool->available;
                }
            }
        }
    }

    ctx.latestReadyFrameIndex = SIZE_MAX;

    const double& timestampPeriodMsec = (double)Engine::graphics()->getPhysicalDeviceLimits().timestampPeriod / 1000000.0;
    for (size_t i = 0; i < ctx.allFrameStartIndexOffsets.size(); ++i) {
        size_t& frameStartIndexOffset = ctx.allFrameStartIndexOffsets[i];
        size_t frameEndIndexOffset = (i < ctx.allFrameStartIndexOffsets.size() - 1)
                                     ? ctx.allFrameStartIndexOffsets[i + 1]
                                     : ctx.allFrameProfiles.size();

        // Sanity check: The first profile of a frame should not have a parent.
        assert(ctx.allFrameProfiles[frameStartIndexOffset].parentIndex == SIZE_MAX);

        uint32_t frameAvailableQueriesCount = 0;

        for (size_t j = frameStartIndexOffset; j < frameEndIndexOffset; ++j) {
            GPUProfile& profile = ctx.allFrameProfiles[j];

            if (profile.startQuery.queryPool != nullptr) {
                if (profile.startQuery.queryPool->available > 0 && profile.startQuery.queryPool->queryResults[profile.startQuery.queryIndex * 2 + 1] != 0) {
                    profile.startQuery.time = (double) profile.startQuery.queryPool->queryResults[profile.startQuery.queryIndex * 2] * timestampPeriodMsec;
                    profile.startQuery.queryPool = nullptr;
                }
            }
            if (profile.endQuery.queryPool != nullptr) {
                if (profile.endQuery.queryPool->available > 0 && profile.endQuery.queryPool->queryResults[profile.endQuery.queryIndex * 2 + 1] != 0) {
                    profile.endQuery.time = (double) profile.endQuery.queryPool->queryResults[profile.endQuery.queryIndex * 2] * timestampPeriodMsec;
                    profile.endQuery.queryPool = nullptr;
                }
            }
            if (profile.startQuery.queryPool == nullptr && profile.endQuery.queryPool == nullptr) {
                ++frameAvailableQueriesCount;
            }
        }

        size_t numProfiles = frameEndIndexOffset - frameStartIndexOffset;
        if (frameAvailableQueriesCount == numProfiles) {
            // All queries were available for the entire frame.
            ctx.latestReadyFrameIndex = i;
        }
    }

    // TODO: we need to clear out the query pools, either delete the completed ones, or better, reuse them. (pool of pools?)

    if (ctx.latestReadyFrameIndex != SIZE_MAX) {
        while (ctx.latestReadyFrameIndex > 0) {
            size_t eraseCount = ctx.allFrameStartIndexOffsets[ctx.latestReadyFrameIndex];
            ctx.allFrameProfiles.erase(ctx.allFrameProfiles.begin(), ctx.allFrameProfiles.begin() + eraseCount);
            ctx.allFrameStartIndexOffsets.erase(ctx.allFrameStartIndexOffsets.begin());
            for (size_t i = 0; i < ctx.allFrameStartIndexOffsets.size(); ++i) {
                ctx.allFrameStartIndexOffsets[i] -= eraseCount;
            }
            --ctx.latestReadyFrameIndex;
        }
    }

    ctx.queryCount = 0;

    ctx.frameStarted = false;
#endif
#endif
}

void Profiler::beginGPU(const profile_id& id, const vk::CommandBuffer& commandBuffer) {
    Engine::graphics()->beginCmdDebugLabel(commandBuffer, id->name);
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();
    if (!ctx.frameStarted)
        return;

    const size_t& startIndex = ctx.allFrameStartIndexOffsets.back();

    size_t parentIndex = ctx.currentIndex;
    ctx.currentIndex = ctx.allFrameProfiles.size() - startIndex;

    GPUProfile* profile = &ctx.allFrameProfiles.emplace_back(GPUProfile{});
    profile->id = id;
    profile->parentIndex = parentIndex;

    if (parentIndex != SIZE_MAX) {
        GPUProfile& parent = ctx.allFrameProfiles[parentIndex + startIndex];
        if (parent.lastChildIndex != SIZE_MAX) {
            GPUProfile& lastChild = ctx.allFrameProfiles[parent.lastChildIndex + startIndex];
            lastChild.nextSiblingIndex = ctx.currentIndex;
        }
        parent.lastChildIndex = ctx.currentIndex;
    }

    writeTimestamp(commandBuffer, vk::PipelineStageFlagBits::eBottomOfPipe, &profile->startQuery);
#endif
#endif
}

void Profiler::endGPU(const vk::CommandBuffer& commandBuffer) {
    Engine::graphics()->endCmdDebugLabel(commandBuffer);
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();
    if (!ctx.frameStarted)
        return;

    const size_t& startIndex = ctx.allFrameStartIndexOffsets.back();

    assert(ctx.currentIndex < ctx.allFrameProfiles.size() - startIndex);
    GPUProfile* profile = &ctx.allFrameProfiles[ctx.currentIndex + startIndex];
    ctx.currentIndex = profile->parentIndex;

    writeTimestamp(commandBuffer, vk::PipelineStageFlagBits::eBottomOfPipe, &profile->endQuery);
#endif
#endif
}

void Profiler::getFrameProfile(std::unordered_map<uint64_t, std::vector<CPUProfile>>& outThreadProfiles) {
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    std::scoped_lock<std::mutex> lock(s_threadContextsMtx);
    for (auto& entry : s_threadContexts) {
        uint64_t threadId = entry.first;
        ThreadContext& ctx = *entry.second;
        if (!ctx.threadActive)
            continue;

        if (ctx.prevFrameProfiles.empty())
            continue;

        std::vector<CPUProfile>& outProfiles = outThreadProfiles[threadId];
        int64_t index = (int64_t)outProfiles.size();

        std::scoped_lock<std::mutex> lock2(ctx.mtx);
        outProfiles.resize(index + ctx.prevFrameProfiles.size());
        std::copy(ctx.prevFrameProfiles.begin(), ctx.prevFrameProfiles.end(), outProfiles.begin() + index);
    }
#endif
}

bool Profiler::getLatestGpuFrameProfile(std::vector<GPUProfile>& outGpuProfiles) {
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();

    if (ctx.latestReadyFrameIndex > ctx.allFrameStartIndexOffsets.size()) {
        return false; // No frames are ready, queries are still pending a response.
    }

    std::scoped_lock<std::mutex> lock(ctx.mtx);

    size_t copySrcIndex = ctx.allFrameStartIndexOffsets[ctx.latestReadyFrameIndex];
    size_t nextCopySrcIndex = (ctx.latestReadyFrameIndex < ctx.allFrameStartIndexOffsets.size() - 1)
            ? ctx.allFrameStartIndexOffsets[ctx.latestReadyFrameIndex + 1]
            : ctx.allFrameProfiles.size();

    assert(copySrcIndex < nextCopySrcIndex);

    size_t copyCount = nextCopySrcIndex - copySrcIndex;
    size_t copyDstIndex = outGpuProfiles.size();

    outGpuProfiles.resize(copyDstIndex + copyCount);
    std::copy(ctx.allFrameProfiles.begin() + copySrcIndex, ctx.allFrameProfiles.begin() + copySrcIndex + copyCount, outGpuProfiles.begin() + copyDstIndex);
#endif
    return true;
}

bool Profiler::isGpuProfilingEnabled() {
    return Engine::graphics()->getPhysicalDeviceLimits().timestampComputeAndGraphics;
}

float Profiler::getGpuProfilingResolutionNanoseconds() {
    // Number of nanoseconds for a GPU timestamp query to increment by 1
    return Engine::graphics()->getPhysicalDeviceLimits().timestampPeriod;
}

bool Profiler::writeTimestamp(const vk::CommandBuffer& commandBuffer, const vk::PipelineStageFlagBits& pipelineStage, GPUQuery* outQuery) {
    assert(outQuery != nullptr);

    GPUContext& ctx = gpuContext();

    outQuery->queryPool = nullptr;
    if (!getGpuQueryPool(1, &outQuery->queryPool)) {
        return false;
    }

    outQuery->queryIndex = outQuery->queryPool->size;
    commandBuffer.writeTimestamp(pipelineStage, outQuery->queryPool->pool, outQuery->queryIndex);
    ++outQuery->queryPool->size;
    ctx.queryCount++;
    return true;
}

bool Profiler::getGpuQueryPool(const uint32_t& numQueries, GPUQueryPool** queryPool) {
    GPUContext& ctx = gpuContext();

    if (ctx.currentQueryPool != nullptr) {
        if (ctx.currentQueryPool->size + numQueries < ctx.currentQueryPool->capacity) {
            *queryPool = ctx.currentQueryPool;
            return true;
        }

//        ctx.queryPoolSize += ctx.queryPoolSize / 2; // Grow by 50%
    }

    std::shared_ptr<vkr::Device> device = Engine::graphics()->getDevice();

    vk::QueryPool queryPoolHandle = VK_NULL_HANDLE;
    if (!createGpuTimestampQueryPool(**device, ctx.queryPoolSize, &queryPoolHandle)) {
        return false;
    }

    Engine::graphics()->setObjectName(**device, (uint64_t)(VkQueryPool)(queryPoolHandle), vk::ObjectType::eQueryPool, "Profiler-GpuTimeQueryPool-" + std::to_string(ctx.queryPools.size()));

//    (**device).resetQueryPool(queryPoolHandle, 0, s_queryPoolSize);

    const vk::CommandBuffer& commandBuffer = Engine::graphics()->beginOneTimeCommandBuffer();
    commandBuffer.resetQueryPool(queryPoolHandle, 0, ctx.queryPoolSize);
    Engine::graphics()->endOneTimeCommandBuffer(commandBuffer, Engine::graphics()->getQueue(QUEUE_GRAPHICS_MAIN));

    ctx.currentQueryPool = ctx.queryPools.emplace_back(new GPUQueryPool());
    ctx.currentQueryPool->device = device;
    ctx.currentQueryPool->pool = queryPoolHandle;
    ctx.currentQueryPool->capacity = ctx.queryPoolSize;
    ctx.currentQueryPool->size = 0;
    *queryPool = ctx.currentQueryPool;
    return true;
}

bool Profiler::createGpuTimestampQueryPool(const vk::Device& device, const uint32_t& capacity, vk::QueryPool* queryPool) {
    printf("createGpuTimestampQueryPool, capacity=%u on frame %llu\n", capacity, Engine::currentFrameCount());
    vk::QueryPoolCreateInfo createInfo{};
    createInfo.flags = {};
    createInfo.queryType = vk::QueryType::eTimestamp;
    createInfo.queryCount = capacity;

    vk::Result result = device.createQueryPool(&createInfo, nullptr, queryPool);
    if (result != vk::Result::eSuccess) {
        printf("Unable to allocate query pool for GPU time stamps: \"%s\"\n", vk::to_string(result).c_str());
        return false;
    }
    return true;
}

void Profiler::onCleanupGraphics(ShutdownGraphicsEvent* event) {
    GPUContext& ctx = gpuContext();
    const vk::Device& device = **Engine::graphics()->getDevice();
    for (const auto& queryPool : ctx.queryPools) {
        device.destroyQueryPool(queryPool->pool);
        delete queryPool;
    }
}

Profiler::ThreadContext& Profiler::threadContext() {
    static thread_local ThreadContext ctx;
    return ctx;
}

Profiler::GPUContext& Profiler::gpuContext() {
    static GPUContext ctx;
    return ctx;
}






ScopeProfiler::ScopeProfiler(profile_id const& id):
    m_currentRegionId(nullptr) {
#if PROFILING_ENABLED
    Profiler::beginCPU(id);
#endif
}

ScopeProfiler::~ScopeProfiler() {
#if PROFILING_ENABLED
    endRegion();
    Profiler::endCPU();
#endif
}

void ScopeProfiler::beginRegion(const profile_id& id) {
#if PROFILING_ENABLED
    endRegion();
    Profiler::beginCPU(id);
    m_currentRegionId = id;
#endif
}

void ScopeProfiler::endRegion() {
#if PROFILING_ENABLED
    if (m_currentRegionId != nullptr) {
        m_currentRegionId = nullptr;
        Profiler::endCPU();
    }
#endif
}
