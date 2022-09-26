
#include "Profiler.h"
#include "core/thread/ThreadUtils.h"
#include "core/application/Application.h"
#include <thread>

thread_local Performance::moment_t Performance::s_lastTime = Performance::now();
thread_local Profiler::ThreadContext Profiler::s_context;
std::unordered_map<uint64_t, Profiler::ThreadContext*> Profiler::s_threadContexts;

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
    Profiler::s_threadContexts.insert(std::make_pair(ThreadUtils::getCurrentThreadHashedId(), this));
}

Profiler::ThreadContext::~ThreadContext() {
//    Profiler::s_threadContexts.erase(ThreadUtils::getCurrentThreadHashedId()); // TODO: why does this cause an exception when closing the application?
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
    __itt_task_begin(domain(), __itt_null, __itt_null, id);

    ThreadContext& ctx = context();
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
}

void Profiler::end() {
    __itt_task_end(domain());

    ThreadContext& ctx = context();
    if (!ctx.frameStarted)
        return; // Ignore constructing frame profiles if this is not part of a frame.

    assert(ctx.currentIndex < ctx.frameProfiles.size());

    Profile& profile = ctx.frameProfiles[ctx.currentIndex];
    profile.endCPU = Performance::now();
    ctx.currentIndex = profile.parentIndex;
}

void Profiler::beginFrame() {
//    printf("======== FRAME BEGIN thread 0x%016x\n", ThreadUtils::getCurrentThreadHashedId());

    ThreadContext& ctx = context();

    ctx.currentIndex = SIZE_MAX;
    ctx.frameProfiles.clear();

    ctx.frameStarted = true;
    static profile_id id = Profiler::id("Frame");
    Profiler::begin(id);
}

void Profiler::endFrame() {
    ThreadContext& ctx = context();

    Profiler::end();
    ctx.frameStarted = false;

    {
        std::unique_lock<std::mutex> lock(ctx.mtx);
        ctx.prevFrameProfiles.swap(ctx.frameProfiles);
    }

//    printf("======== FRAME END thread 0x%016x\n", ThreadUtils::getCurrentThreadHashedId());
//
//    size_t numFrames = 1;
//    size_t start = ctx.frames.size() >= numFrames ? ctx.frames.size() - numFrames : 0;
//    for (size_t i = start; i < ctx.frames.size(); ++i) {
//        printf("Thread 0x%016x, Frame %llu :: %llu profiles, %.2f msec\n", ThreadUtils::getCurrentThreadHashedId(), i, ctx.frames[i].profiles.size(), Performance::milliseconds(ctx.frames[i].profiles[0].startCPU, ctx.frames[i].profiles[0].endCPU));
//    }

//    size_t parentIndex = SIZE_MAX;
//    size_t depth = 0;
//    char indent[256];
//    const wchar_t* msecStr = L"msec";
//    const wchar_t* usecStr = L"μsec";
//
//    printf("======== END FRAME\n");
//    for (size_t i = 0; i < ctx.frameProfiles.size(); ++i) {
//        Profile& profile = ctx.frameProfiles[i];
//
//        if (profile.parentIndex != SIZE_MAX) {
//            if (profile.parentIndex > parentIndex || parentIndex == SIZE_MAX) {
//                indent[depth] = ' ';
//                ++depth;
//            } else {
//                while (profile.parentIndex < parentIndex && parentIndex != SIZE_MAX) {
//                    parentIndex = ctx.frameProfiles[parentIndex].parentIndex;
//                    --depth;
//                    indent[depth] = '\0';
//                }
//            }
//        }
//
//        double time = Performance::milliseconds(profile.startCPU, profile.endCPU);
//        const wchar_t* timeStr = msecStr;
//        if (time < 1.0) {
//            time *= 1000.0;
//            timeStr = usecStr;
//        }
//
//        if (time >= 0.1) {
//            printf("%s%s - %.2f %ls\n", indent, profile.id->strA, time, timeStr);
//        }
//
//        parentIndex = profile.parentIndex;
//    }
}

Profiler::ThreadContext& Profiler::context() {
    return s_context;
}

void Profiler::flushFrames() {
    // TODO: write performance log to file?
}

void Profiler::getFrameProfile(std::unordered_map<uint64_t, std::vector<Profile>>& outThreadProfiles) {
    for (auto it = s_threadContexts.begin(); it != s_threadContexts.end(); ++it) {
        uint64_t threadId = it->first;
        ThreadContext& ctx = *it->second;

        std::vector<Profile>& outProfiles = outThreadProfiles[threadId];
        size_t index = outProfiles.size();

        std::unique_lock<std::mutex> lock(ctx.mtx);
        outProfiles.resize(index + ctx.prevFrameProfiles.size());
        memcpy(&outProfiles[index], &ctx.prevFrameProfiles[0], sizeof(Profile) * ctx.prevFrameProfiles.size());
    }
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
