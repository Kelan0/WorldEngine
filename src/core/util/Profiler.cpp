
#include "Profiler.h"
#include "core/thread/ThreadUtils.h"
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/engine/scene/event/EventDispatcher.h"
#include "core/engine/scene/event/Events.h"
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
    for (int i = 0; i < CONCURRENT_FRAMES; ++i)
        queryData.set(i, new GPUQueryFrameData());
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

template<typename Profile, typename Context>
Profile* PushProfile(const profile_id& id, Context& ctx) {
    size_t parentIndex = ctx.currentIndex;
    ctx.currentIndex = ctx.frameProfiles.size();

    Profile* profile = &ctx.frameProfiles.emplace_back();
    profile->id = id;
    profile->parentIndex = parentIndex;

    if (parentIndex != SIZE_MAX) {
        Profile& parent = ctx.frameProfiles[parentIndex];
        if (parent.lastChildIndex != SIZE_MAX) {
            Profile& lastChild = ctx.frameProfiles[parent.lastChildIndex];
            lastChild.nextSiblingIndex = ctx.currentIndex;
        }
        parent.lastChildIndex = ctx.currentIndex;
    }

    return profile;
}


template<typename Profile, typename Context>
Profile* PopProfile(Context& ctx) {
    assert(ctx.currentIndex < ctx.frameProfiles.size());
    Profile* profile = &ctx.frameProfiles[ctx.currentIndex];
    ctx.currentIndex = profile->parentIndex;
    return profile;
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

    CPUProfile* profile = PushProfile<CPUProfile>(id, ctx);
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

    CPUProfile* profile = PopProfile<CPUProfile>(ctx);
    profile->endTime = Performance::now();
#endif
#endif
}

void Profiler::beginGPU(const profile_id& id) {
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();
    if (!ctx.frameStarted)
        return;

    PushProfile<GPUProfile>(id, ctx);
#endif
#endif
}

void Profiler::endGPU() {
#if PROFILING_ENABLED
#if INTERNAL_PROFILING_ENABLED
    GPUContext& ctx = gpuContext();
    if (!ctx.frameStarted)
        return;

    PopProfile<GPUProfile>(ctx);
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
        size_t index = outProfiles.size();

        std::scoped_lock<std::mutex> lock2(ctx.mtx);
        outProfiles.resize(index + ctx.prevFrameProfiles.size());
        std::copy(ctx.prevFrameProfiles.begin(), ctx.prevFrameProfiles.end(), outProfiles.begin() + index);
    }
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
    outQuery->queryIndex = ctx.queryData->queryCount;

    outQuery->queryPool = nullptr;
    if (!getGpuQueryPool(outQuery->queryIndex + 1, &outQuery->queryPool)) {
        return false;
    }

    commandBuffer.writeTimestamp(pipelineStage, outQuery->queryPool->pool, outQuery->queryIndex);
    ctx.queryData->queryCount++;
    return true;
}

bool Profiler::getGpuQueryPool(const uint32_t& maxQueries, GPUQueryPool** queryPool) {
    GPUContext& ctx = gpuContext();
    if (!ctx.queryData->queryPools.empty()) {
        GPUQueryPool* selectedQueryPool = &ctx.queryData->queryPools.back();
        if (selectedQueryPool->capacity <= maxQueries * 2 && selectedQueryPool->size < selectedQueryPool->capacity) {
            *queryPool = selectedQueryPool;
            return true;
        }
    }

    uint32_t capacity = INT_DIV_CEIL(maxQueries * 2, 1000) * 1000;
    vk::QueryPool vkQueryPool = VK_NULL_HANDLE;
    if (!createGpuTimestampQueryPool(capacity, &vkQueryPool)) {
        return false;
    }

    GPUQueryPool* newQueryPool = &ctx.queryData->queryPools.emplace_back(GPUQueryPool{});
    newQueryPool->pool = vkQueryPool;
    newQueryPool->capacity = capacity;
    newQueryPool->size = 0;
    *queryPool = newQueryPool;
    return true;
}

bool Profiler::createGpuTimestampQueryPool(const uint32_t& capacity, vk::QueryPool* queryPool) {
    printf("createGpuTimestampQueryPool, capacity=%u\n", capacity);
    vk::QueryPoolCreateInfo createInfo{};
    createInfo.flags = {};
    createInfo.queryType = vk::QueryType::eTimestamp;
    createInfo.queryCount = capacity;

    const vk::Device& device = **Engine::graphics()->getDevice();
    vk::Result result = device.createQueryPool(&createInfo, nullptr, queryPool);
    if (result != vk::Result::eSuccess) {
        printf("Unable to allocate query pool for GPU time stamps: \"%s\"\n", vk::to_string(result).c_str());
        return false;
    }
    return true;
}

void Profiler::onCleanupGraphics(const ShutdownGraphicsEvent& event) {
    GPUContext& ctx = gpuContext();
    const vk::Device& device = **Engine::graphics()->getDevice();
    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        for (const auto& queryPool : ctx.queryData[i]->queryPools) {
            device.destroyQueryPool(queryPool.pool);
        }
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
