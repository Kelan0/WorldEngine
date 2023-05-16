
#include "Profiler.h"
#include "core/thread/ThreadUtils.h"
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include <thread>

uint32_t nextQueryPoolId = 1;

std::string stringListIds(const std::vector<uint32_t>& ids) {
    std::string str;
    for (size_t i = 0; i < ids.size(); ++i)
        str += "ID" + std::to_string(ids[i]) + ((i < ids.size() - 1) ? ", " : "");
    return str;
}

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
    printf("Profiler::ThreadContext() - 0x%08llX\n", ThreadUtils::getCurrentThreadHashedId());
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    uint64_t currentId = ThreadUtils::getCurrentThreadHashedId();

    std::scoped_lock<std::mutex> lock(s_threadContextsMtx);
    s_threadContexts.insert(std::make_pair(currentId, this));

#endif
}

Profiler::ThreadContext::~ThreadContext() {
    printf("Profiler::~ThreadContext() - 0x%08llX\n", ThreadUtils::getCurrentThreadHashedId());
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
    printf("Creating profiler GPUContext on thread (0x%016llx)\n", ThreadUtils::getCurrentThreadHashedId());
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

void Profiler::beginGraphicsFrame() {
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();
    assert(ctx.profileStackDepth == 0 && "Profile stack incomplete");

    ctx.currentIndex = SIZE_MAX;
//    ctx.frameProfiles.clear(); // TODO: Remove oldest frame profiles that have a query response.
    ctx.allFrameStartIndexOffsets.emplace_back(ctx.allFrameProfiles.size());
    ctx.frameStarted = true;
#endif
#endif
}

void Profiler::endGraphicsFrame() {
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();

//    std::vector<uint32_t> ids;
//    std::set<uint32_t> uniqueIds;

#if _DEBUG
    bool debugAllProfilesClosedCheck = true;
    for (const auto& entry : ctx.debugOpenProfiles) {
        const int32_t& openCount = entry.second;
        if (openCount != 0) {
            printf("Open graphics profile \"%s\" was not closed\n", entry.first.c_str());
            debugAllProfilesClosedCheck = false;
        }
    }
    assert(debugAllProfilesClosedCheck && "Not all open graphics profiles were closed (not every beginGPU() has a matching endGPU() call");
#endif

    assert(ctx.profileStackDepth == 0 && "Profile stack incomplete");


    for (const auto& queryPool : ctx.destroyedQueryPools) {
//        ids.emplace_back(queryPool->id);
        destroyQueryPool(queryPool);
    }
    ctx.destroyedQueryPools.clear();

//    if (!ids.empty())
//        printf("[%s] %llu query pools destroyed\n", stringListIds(ids).c_str(), ids.size());
    if (ctx.allFrameStartIndexOffsets.size() > 10) {
        printf("?\n");
    }

    const vk::Device& device = **Engine::graphics()->getDevice();

    vk::Result result;


    vk::QueryResultFlags flags = vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWithAvailability;

    // Reset currentQueryPoolIndex so that the first call to getNextQueryPool will first look in the unusedQueryPools list.
    ctx.currentQueryPoolIndex = SIZE_MAX;

    size_t dataSize;
    uint32_t strideBytes = sizeof(uint64_t) * 2;

    uint32_t debugPoolsAvailableCount = 0;

//    ids.clear();

    // Loop over all active query pools and try to retrieve the results.
    for (auto& queryPool : ctx.queryPools) {
        if (!queryPool->allAvailable) {
            queryPool->queryResults.resize(queryPool->size * 2);
            dataSize = queryPool->queryResults.size() * sizeof(uint64_t);
            result = device.getQueryPoolResults(queryPool->pool, 0, queryPool->size, dataSize, queryPool->queryResults.data(), (vk::DeviceSize)strideBytes, flags);

            queryPool->allAvailable = true;
            for (uint32_t i = 0; i < queryPool->size; ++i) {
                if (queryPool->queryResults[i * 2 + 1] == 0) {
                    queryPool->allAvailable = false;
                }
            }
//            if (queryPool->allAvailable) {
//                ids.emplace_back(queryPool->id);
//                ++debugPoolsAvailableCount;
//            }
        }
    }

//    if (debugPoolsAvailableCount > 0)
//        printf("[%s] %u QueryPools became fully available this frame\n", stringListIds(ids).c_str(), debugPoolsAvailableCount);

    ctx.latestReadyFrameIndex = SIZE_MAX;

//    uniqueIds.clear();

    // Loop over all pending frames and try to retrieve the results.
    double timestampPeriodMsec = (double)Engine::graphics()->getPhysicalDeviceLimits().timestampPeriod / 1000000.0;
    for (size_t i = 0; i < ctx.allFrameStartIndexOffsets.size(); ++i) {
        size_t& frameStartIndexOffset = ctx.allFrameStartIndexOffsets[i];
        size_t frameEndIndexOffset = (i < ctx.allFrameStartIndexOffsets.size() - 1)
                                     ? ctx.allFrameStartIndexOffsets[i + 1]
                                     : ctx.allFrameProfiles.size();

        size_t frameProfileCount = frameEndIndexOffset - frameStartIndexOffset;

        // Sanity check: The first profile of a frame should not have a parent.
        assert(ctx.allFrameProfiles[frameStartIndexOffset].parentIndex == SIZE_MAX);

        bool allQueriesAvailable = true;

        for (size_t j = frameStartIndexOffset; j < frameEndIndexOffset; ++j) {
            GPUProfile& profile = ctx.allFrameProfiles[j];

//            if (profile.startQuery.queryPool == nullptr || profile.endQuery.queryPool == nullptr) {
//                // One or both queries were not set, and so cannot be available this frame.
//                allQueriesAvailable = false;
//            }

            if (profile.startQuery.queryPool != nullptr) {
                if (profile.startQuery.queryPool->allAvailable) {
                    assert(profile.startQuery.queryIndex < UINT32_MAX);
                    assert(profile.startQuery.queryPool->queryResults[profile.startQuery.queryIndex * 2 + 1] != 0);
//                    uniqueIds.insert(profile.startQuery.queryPool->id);
                    profile.startQuery.time = (double)profile.startQuery.queryPool->queryResults[profile.startQuery.queryIndex * 2] * timestampPeriodMsec;
                    profile.startQuery.queryPool = nullptr;
#if _DEBUG
                    profile.startQuery.queryReceived = true;
#endif
                }
            }
            if (profile.endQuery.queryPool != nullptr) {
                if (profile.endQuery.queryPool->allAvailable) {
                    assert(profile.endQuery.queryIndex < UINT32_MAX);
                    assert(profile.endQuery.queryPool->queryResults[profile.endQuery.queryIndex * 2 + 1] != 0);
//                    uniqueIds.insert(profile.endQuery.queryPool->id);
                    profile.endQuery.time = (double)profile.endQuery.queryPool->queryResults[profile.endQuery.queryIndex * 2] * timestampPeriodMsec;
                    profile.endQuery.queryPool = nullptr;
#if _DEBUG
                    profile.endQuery.queryReceived = true;
#endif
                }
            }

            if (profile.endQuery.time < profile.startQuery.time) {
                profile.endQuery.time = profile.startQuery.time;
            }

            if (profile.startQuery.queryPool != nullptr || profile.endQuery.queryPool != nullptr) {
                // After reading the value of both queries, the pool is reset back to null. If this didn't happen for both, then the queries are not all available.
                allQueriesAvailable = false;
            }
        }

        if (allQueriesAvailable) {
            ctx.latestReadyFrameIndex = i;
        }
    }

//    if (!uniqueIds.empty()) {
//        ids.assign(uniqueIds.begin(), uniqueIds.end());
//        printf("[%s] %llu query pools were fully read back\n", stringListIds(ids).c_str(), uniqueIds.size());
//    }

    size_t requiredFrameCount = 0;

    // Cleanup data for all frames before latestReadyFrameIndex, and update indices
    if (ctx.latestReadyFrameIndex != SIZE_MAX) {
//        printf("Latest ready frame index is %llu\n", ctx.latestReadyFrameIndex);

        size_t numFramesDeleted = 0;
        size_t numProfilesDeleted = 0;
        if (ctx.latestReadyFrameIndex > 0) {
            size_t eraseCount = ctx.allFrameStartIndexOffsets[ctx.latestReadyFrameIndex];
            ctx.allFrameProfiles.erase(ctx.allFrameProfiles.begin(), ctx.allFrameProfiles.begin() + eraseCount);
            ctx.allFrameStartIndexOffsets.erase(ctx.allFrameStartIndexOffsets.begin(), ctx.allFrameStartIndexOffsets.begin() + ctx.latestReadyFrameIndex);
            for (size_t i = 0; i < ctx.allFrameStartIndexOffsets.size(); ++i)
                ctx.allFrameStartIndexOffsets[i] -= eraseCount;
            numFramesDeleted += ctx.latestReadyFrameIndex;
            numProfilesDeleted += eraseCount;
            ctx.latestReadyFrameIndex = 0;
        }
        requiredFrameCount = ctx.allFrameProfiles.size() - ctx.allFrameStartIndexOffsets[ctx.latestReadyFrameIndex];
//        printf("Storing previous %llu frames, latestReadyFrameIndex=%llu (%llu frames ago). Data for %llu frames were removed (%llu profiles)\n", ctx.allFrameStartIndexOffsets.size(), ctx.latestReadyFrameIndex, ctx.allFrameStartIndexOffsets.size() - ctx.latestReadyFrameIndex - 1, numFramesDeleted, numProfilesDeleted);
    } else {
        requiredFrameCount = ctx.allFrameProfiles.size();
    }

    // Grow minimum query pool size for next frame if current frame exceeded it.
    uint32_t requiredQueryCount = (uint32_t)requiredFrameCount * 2;
//    printf("Required query count is %u, current minimum is %u\n", requiredQueryCount, ctx.minQueryPoolSize);
    if (requiredQueryCount >= ctx.minQueryPoolSize) {
//        ctx.minQueryPoolSize = CEIL_TO_MULTIPLE(requiredQueryCount + ctx.minQueryPoolSize / 2, 500);
        ctx.minQueryPoolSize = requiredQueryCount + ctx.minQueryPoolSize / 2;
//        printf("Growing minQueryPoolSize to %u\n", ctx.minQueryPoolSize);
    }

//    ids.clear();
    // Delete all unused query pools which have a smaller capacity than ctx.minQueryPoolSize.
    auto it0 = ctx.unusedQueryPools.begin();
    for (; it0 != ctx.unusedQueryPools.end(); ++it0) {
        GPUQueryPool* queryPool = *it0;
        if (queryPool->capacity >= ctx.minQueryPoolSize)
            break; // The list is sorted, so stop as soon as we find the first one large enough
        ctx.destroyedQueryPools.emplace_back(queryPool);
//        ids.emplace_back(queryPool->id);
    }
//    if (it0 != ctx.unusedQueryPools.begin())
//        printf("[%s] %lld unused query pools were smaller than the minimum size and deleted\n", stringListIds(ids).c_str(), (int64_t)std::distance(ctx.unusedQueryPools.begin(), it0));
    ctx.unusedQueryPools.erase(ctx.unusedQueryPools.begin(), it0);

//    ids.clear();

//    uint32_t debugNumDestroyed = 0;
    // Remove all newly unused query pools from the queryPools list (either moved to unused list, or deleted if too small)
    for (auto it1 = ctx.queryPools.begin(); it1 != ctx.queryPools.end();) {
        GPUQueryPool* queryPool = *it1;
        if (queryPool->allAvailable) {
            // All queries within this QueryPool were made available and read back. This query pool is no longer being used.
            it1 = ctx.queryPools.erase(it1);

            if (queryPool->capacity >= ctx.minQueryPoolSize) {
                // This QueryPool has a large enough capacity to be reused, insert into the sorted list of unused query pools.
                auto it2 = std::upper_bound(ctx.unusedQueryPools.begin(), ctx.unusedQueryPools.end(), queryPool, [](const GPUQueryPool* lhs, const GPUQueryPool* rhs) {
                    return lhs->capacity < rhs->capacity;
                });
                ctx.unusedQueryPools.insert(it2, queryPool);
            } else {
                // This QueryPool is no longer large enough to be reused, so delete it. If none are available when needed, a new one will be created.
                ctx.destroyedQueryPools.emplace_back(queryPool);
//                ids.emplace_back(queryPool->id);
//                ++debugNumDestroyed;
            }
        } else {
            ++it1;
        }
    }
//    if (debugNumDestroyed > 0)
//        printf("[%s] %u used query pools were smaller than the minimum size and deleted\n", stringListIds(ids).c_str(), debugNumDestroyed);

    ctx.frameStarted = false;

//    printf("EndGraphicsFrame: %llu query pools allocated, %llu profiles stored, %llu frames stored\n\n", ctx.queryPools.size() + ctx.unusedQueryPools.size(), ctx.allFrameProfiles.size(), ctx.allFrameStartIndexOffsets.size());
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

#if _DEBUG
    ++ctx.debugOpenProfiles[id->name];
#endif

    assert(ctx.profileStackDepth < PROFILE_GPU_STACK_LIMIT && "GPU profile stack overflow");
    ++ctx.profileStackDepth;

    size_t startIndex = ctx.allFrameStartIndexOffsets.back();

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

void Profiler::endGPU(const std::string& profileName, const vk::CommandBuffer& commandBuffer) {
    Engine::graphics()->endCmdDebugLabel(commandBuffer);
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();
    if (!ctx.frameStarted)
        return;

#if _DEBUG
    --ctx.debugOpenProfiles[profileName];
#endif

    assert(ctx.profileStackDepth >= 0 && "GPU Profile stack underflow");
    --ctx.profileStackDepth;

    size_t startIndex = ctx.allFrameStartIndexOffsets.back();

    assert(ctx.currentIndex < ctx.allFrameProfiles.size() - startIndex);
    GPUProfile* profile = &ctx.allFrameProfiles[ctx.currentIndex + startIndex];
    ctx.currentIndex = profile->parentIndex;

    writeTimestamp(commandBuffer, vk::PipelineStageFlagBits::eBottomOfPipe, &profile->endQuery);
#endif
#endif
}

void Profiler::getFrameProfile(std::unordered_map<uint64_t, std::vector<CPUProfile>>& outThreadProfiles) {
    PROFILE_SCOPE("Profiler::getFrameProfile")
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
        int32_t index = (int32_t)outProfiles.size();

        std::scoped_lock<std::mutex> lock2(ctx.mtx);
        outProfiles.resize(index + ctx.prevFrameProfiles.size());
        std::copy(ctx.prevFrameProfiles.begin(), ctx.prevFrameProfiles.end(), outProfiles.begin() + index);
    }
#endif
}

bool Profiler::getLatestGpuFrameProfile(std::vector<GPUProfile>& outGpuProfiles) {
    PROFILE_SCOPE("Profiler::getLatestGpuFrameProfile")
#if PROFILING_ENABLED && INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();

    if (ctx.latestReadyFrameIndex > ctx.allFrameStartIndexOffsets.size())
        return false; // No frames are ready, queries are still pending a response.

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
    return true;
#else
    return false;
#endif
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
    if (!getNextQueryPool(&outQuery->queryPool)) {
#if _DEBUG
        outQuery->failedToGetQueryPool = true;
#endif
        return false;
    }

    outQuery->queryIndex = outQuery->queryPool->size;
    commandBuffer.writeTimestamp(pipelineStage, outQuery->queryPool->pool, outQuery->queryIndex);
    ++outQuery->queryPool->size;

#if _DEBUG
    outQuery->queryWritten = true;
#endif
    return true;
}

bool Profiler::getNextQueryPool(GPUQueryPool** queryPool) {
    GPUContext& ctx = gpuContext();

    GPUQueryPool* selectedQueryPool = nullptr;
//    if (ctx.queryPools.empty() && ctx.unusedQueryPools.empty())
//        printf("Profiler::getNextQueryPool - No existing query pools\n");

    if (ctx.currentQueryPoolIndex < ctx.queryPools.size()) {
        // We are using an existing query pool
        selectedQueryPool = ctx.queryPools[ctx.currentQueryPoolIndex];

        if (selectedQueryPool->size + 1 >= selectedQueryPool->capacity) {
            // The existing query pool was full.
//            printf("Current query pool at index %llu (ID%u) is full\n", ctx.currentQueryPoolIndex, selectedQueryPool->id);
            selectedQueryPool = nullptr;
        }
    }

    if (selectedQueryPool == nullptr && !ctx.unusedQueryPools.empty()) {
        selectedQueryPool = ctx.unusedQueryPools.back();
        ctx.unusedQueryPools.pop_back();
        assert(selectedQueryPool->capacity >= ctx.minQueryPoolSize);

        ctx.currentQueryPoolIndex = ctx.queryPools.size();
        ctx.queryPools.emplace_back(selectedQueryPool);
//        printf("Selected unused query pool ID%u at index %llu\n", selectedQueryPool->id, ctx.currentQueryPoolIndex);
        resetQueryPools(&selectedQueryPool, 1);
    }

    if (selectedQueryPool == nullptr) {
        const SharedResource<vkr::Device>& device = Engine::graphics()->getDevice();

        vk::QueryPool queryPoolHandle = nullptr;
        if (!createGpuTimestampQueryPool(**device, ctx.minQueryPoolSize, &queryPoolHandle)) {
            return false;
        }

        uint32_t id = nextQueryPoolId++;
        Engine::graphics()->setObjectName(**device, (uint64_t)(VkQueryPool)(queryPoolHandle), vk::ObjectType::eQueryPool, "Profiler-GpuTimeQueryPool-" + std::to_string(id));

//        printf("Allocated new query pool ID%u, capacity=%u on frame %llu\n", id, ctx.minQueryPoolSize, Engine::currentFrameCount());

        ctx.currentQueryPoolIndex = ctx.queryPools.size();
        selectedQueryPool = ctx.queryPools.emplace_back(new GPUQueryPool());
        selectedQueryPool->device = device;
        selectedQueryPool->pool = queryPoolHandle;
        selectedQueryPool->capacity = ctx.minQueryPoolSize;
        selectedQueryPool->size = ctx.minQueryPoolSize; // Pretend all queries are used to force a reset
        selectedQueryPool->id = id;
        resetQueryPools(&selectedQueryPool, 1);
    }

    *queryPool = selectedQueryPool;
    return true;
}

bool Profiler::createGpuTimestampQueryPool(const vk::Device& device, uint32_t capacity, vk::QueryPool* queryPool) {
    vk::QueryPoolCreateInfo createInfo{};
    createInfo.flags = {};
    createInfo.queryType = vk::QueryType::eTimestamp;
    createInfo.queryCount = capacity;

    vk::Result result = device.createQueryPool(&createInfo, nullptr, queryPool);
    if (result != vk::Result::eSuccess) {
//        printf("Unable to allocate query pool for GPU time stamps: \"%s\"\n", vk::to_string(result).c_str());
        return false;
    }
    return true;
}

void Profiler::destroyQueryPool(GPUQueryPool* queryPool) {
    const vk::Device& device = **queryPool->device.get();
    device.destroyQueryPool(queryPool->pool, nullptr);
    delete queryPool;
}

void Profiler::resetQueryPools(GPUQueryPool** queryPools, size_t count) {
    GPUContext& ctx = gpuContext();
    vk::CommandBuffer commandBuffer = nullptr;

    uint32_t numReset = 0;

    std::vector<uint32_t> ids;

    for (size_t i = 0; i < count; ++i) {
        GPUQueryPool* queryPool = queryPools[i];
        if (queryPool == nullptr || queryPool->size == 0 || queryPool->capacity < ctx.minQueryPoolSize)
            continue;

        if (!commandBuffer) // Begin the command buffer only if there was something to reset
            commandBuffer = Engine::graphics()->beginOneTimeCommandBuffer();

//        const vk::Device& device = **queryPool->device.lock();
//        device.resetQueryPool(queryPool->pool, 0, queryPool->capacity);
        commandBuffer.resetQueryPool(queryPool->pool, 0, queryPool->capacity);
        queryPool->size = 0;
        queryPool->allAvailable = false;
        ids.emplace_back(queryPool->id);
        ++numReset;
    }

//    if (numReset > 0)
//        printf("Resetting %u query pools [%s]\n", numReset, stringListIds(ids).c_str());

    if (commandBuffer)
        Engine::graphics()->endOneTimeCommandBuffer(commandBuffer, Engine::graphics()->getQueue(QUEUE_GRAPHICS_MAIN));
}

void Profiler::onCleanupGraphics(ShutdownGraphicsEvent* event) {
    GPUContext& ctx = gpuContext();
    for (const auto& queryPool : ctx.queryPools)
        destroyQueryPool(queryPool);
    for (const auto& queryPool : ctx.unusedQueryPools)
        destroyQueryPool(queryPool);
    for (const auto& queryPool : ctx.destroyedQueryPools)
        destroyQueryPool(queryPool);
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
