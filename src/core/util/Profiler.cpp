
#include "Profiler.h"
#include "core/thread/ThreadUtils.h"
#include "core/application/Application.h"
#include <thread>

thread_local Performance::moment_t Performance::s_lastTime = Performance::now();
#if PROFILING_ENABLED
thread_local Profiler::ThreadContext Profiler::s_context;
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
    return nanoseconds(duration) / 1000000.0;
}

double Performance::milliseconds(const moment_t& startTime, const moment_t& endTime) {
    return milliseconds(endTime - startTime);
}

double Performance::milliseconds(const moment_t& startTime) {
    return milliseconds(now() - startTime);
}




Profiler::ThreadContext::ThreadContext() {
#if PROFILING_ENABLED
    std::scoped_lock<std::mutex> lock(s_threadContextsMtx);
    s_threadContexts.insert(std::make_pair(ThreadUtils::getCurrentThreadHashedId(), this));
#endif
}

Profiler::ThreadContext::~ThreadContext() {
#if PROFILING_ENABLED
    std::scoped_lock<std::mutex> lock(s_threadContextsMtx);
    s_threadContexts.erase(ThreadUtils::getCurrentThreadHashedId()); // TODO: why does this cause an exception when closing the application?
#endif
}

Profiler::Profiler() {}

Profiler::~Profiler() {}

profile_id Profiler::id(const char* name) {
    return __itt_string_handle_createA(name);
}

__itt_domain* Profiler::domain() {
    static __itt_domain* s_domain = __itt_domain_createA(PROFILE_DOMAIN_NAME);
    return s_domain;
}

void Profiler::begin(profile_id const& id) {
#if PROFILING_ENABLED
    __itt_task_begin(domain(), __itt_null, __itt_null, id);

    ThreadContext& ctx = s_context;
    if (!ctx.frameStarted)
        return; // Ignore constructing frame profiles if this is not part of a frame.

    size_t parentIndex = ctx.currentIndex;
    ctx.currentIndex = ctx.frameProfiles.size();

    Profile& profile = ctx.frameProfiles.emplace_back();
    profile.id = id;
    profile.parentIndex = parentIndex;

    if (parentIndex != SIZE_MAX) {
        Profile& parent = ctx.frameProfiles[parentIndex];
        if (parent.lastChildIndex != SIZE_MAX) {
            Profile& lastChild = ctx.frameProfiles[parent.lastChildIndex];
            lastChild.nextSiblingIndex = ctx.currentIndex;
        }
        parent.lastChildIndex = ctx.currentIndex;
    }

    profile.startCPU = Performance::now();
#endif
}

void Profiler::end() {
#if PROFILING_ENABLED
    __itt_task_end(domain());

    ThreadContext& ctx = s_context;
    if (!ctx.frameStarted)
        return; // Ignore constructing frame profiles if this is not part of a frame.

    assert(ctx.currentIndex < ctx.frameProfiles.size());

    Profile& profile = ctx.frameProfiles[ctx.currentIndex];
    profile.endCPU = Performance::now();
    ctx.currentIndex = profile.parentIndex;
#endif
}

void Profiler::beginFrame() {
#if PROFILING_ENABLED
//    printf("======== FRAME BEGIN thread 0x%016x\n", ThreadUtils::getCurrentThreadHashedId());

    ThreadContext& ctx = s_context;
    ctx.threadActive = true;

    ctx.currentIndex = SIZE_MAX;
    ctx.frameProfiles.clear();

    ctx.frameStarted = true;
    static profile_id id = Profiler::id("Frame");
    Profiler::begin(id);
#endif
}

void Profiler::endFrame() {
#if PROFILING_ENABLED
    ThreadContext& ctx = s_context;

    Profiler::end();
    ctx.frameStarted = false;

    {
        std::scoped_lock<std::mutex> lock(ctx.mtx);
        ctx.prevFrameProfiles.swap(ctx.frameProfiles);
    }
#endif
}

void Profiler::flushFrames() {
#if PROFILING_ENABLED
    // TODO: write performance log to file?
#endif
}

void Profiler::getFrameProfile(std::unordered_map<uint64_t, std::vector<Profile>>& outThreadProfiles) {
#if PROFILING_ENABLED
    std::scoped_lock<std::mutex> lock(s_threadContextsMtx);
    for (auto it = s_threadContexts.begin(); it != s_threadContexts.end(); ++it) {
        uint64_t threadId = it->first;
        ThreadContext& ctx = *it->second;
        if (!ctx.threadActive)
            continue;

        if (ctx.prevFrameProfiles.empty())
            continue;

        std::vector<Profile>& outProfiles = outThreadProfiles[threadId];
        size_t index = outProfiles.size();

        std::scoped_lock<std::mutex> lock2(ctx.mtx);
        outProfiles.resize(index + ctx.prevFrameProfiles.size());
        //memcpy(&outProfiles[index], &ctx.prevFrameProfiles[0], sizeof(Profile) * ctx.prevFrameProfiles.size());
        std::copy(ctx.prevFrameProfiles.begin(), ctx.prevFrameProfiles.end(), outProfiles.begin() + index);
    }
#endif
}


ScopeProfiler::ScopeProfiler(profile_id const& id):
    m_currentRegionId(nullptr) {
#if PROFILING_ENABLED
    Profiler::begin(id);
#endif
}

ScopeProfiler::~ScopeProfiler() {
#if PROFILING_ENABLED
    endRegion();
    Profiler::end();
#endif
}

void ScopeProfiler::beginRegion(const profile_id& id) {
#if PROFILING_ENABLED
    endRegion();
    Profiler::begin(id);
    m_currentRegionId = id;
#endif
}

void ScopeProfiler::endRegion() {
#if PROFILING_ENABLED
    if (m_currentRegionId != nullptr) {
        m_currentRegionId = nullptr;
        Profiler::end();
    }
#endif
}
