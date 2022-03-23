
#ifndef WORLDENGINE_THREADUTILS_H
#define WORLDENGINE_THREADUTILS_H

#include "core/thread/WorkerThread.h"
#include "core/thread/ThreadPool.h"

namespace ThreadUtils {
    extern ThreadPool* POOL;

    template<typename Func, typename... Args>
    void parallel_range(size_t range, Func&& func, Args&&... args);

    template<typename Func, typename... Args>
    void parallel_for(size_t count, Func&& func, Args&&... args);

    template<typename Iter, typename Func>
    void parallel_for(Iter first, Iter last, Func&& func);
}


template<typename Func, typename... Args>
void ThreadUtils::parallel_range(size_t range, Func&& func, Args&&... args) {
    printf("=== parallel_range start\n");

    Profiler pf; double msec;

    pf.mark();

    size_t threadCount = POOL->getThreadCount();
    size_t rangePerThread = INT_DIV_CEIL(range, threadCount);

    using return_t = invoke_result_t<Func, size_t, size_t, Args...>;

    std::vector<std::future<return_t>> results;
    results.reserve(threadCount);

    for (size_t i = 0; i < threadCount; ++i) {
        size_t rangeStart = i * rangePerThread;
        size_t rangeEnd = rangeStart + rangePerThread;
        if (rangeEnd >= range)
            rangeEnd = range;
        if (rangeStart == rangeEnd)
            break;

        printf("=== Task %d\n", i);
        results.push_back(POOL->run(func, rangeStart, rangeEnd, std::forward<Args>(args)...));
    }

    msec = pf.markMsec();
    printf("[Thread %llu] ThreadUtils::parallel_range - Took %.5f msec to dispatch %d tasks\n", std::this_thread::get_id(), msec, results.size());

    pf.mark();
    for (size_t i = 0; i < results.size(); ++i) {
        results[i].wait();
    }

    msec = pf.markMsec();
    printf("[Thread %llu] ThreadUtils::parallel_range - Took %.5f msec to wait for tasks to complete\n", std::this_thread::get_id(), msec, results.size());
    printf("=== parallel_range end\n");
}

template<typename Func, typename... Args>
void ThreadUtils::parallel_for(size_t count, Func&& func, Args&&... args) {
    Profiler pf; double msec;

    pf.mark();
    parallel_range(count, [](size_t rangeStart, size_t rangeEnd, auto&& func, auto&&... args) {
        for (size_t i = rangeStart; i != rangeEnd; ++i) {
            func(i, std::forward<Args>(args)...);
        }
    }, std::forward<Func>(func), std::forward<Args>(args)...);

    msec = pf.markMsec();
    printf("[Thread %llu] ThreadUtils::parallel_for - Took %.5f msec to dispatch and execute %d iterations\n", std::this_thread::get_id(), msec, count);
}

template<typename Iter, typename Func>
void ThreadUtils::parallel_for(Iter first, Iter last, Func&& func) {

}


#endif //WORLDENGINE_THREADUTILS_H
