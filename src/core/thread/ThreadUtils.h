
#ifndef WORLDENGINE_THREADUTILS_H
#define WORLDENGINE_THREADUTILS_H

#include "core/thread/ThreadPool.h"

namespace ThreadUtils {
    template<typename Func, typename... Args>
    typename Task<Func, Args...>::future_t run(Task<Func, Args...>* task);

    template<typename Func, typename... Args>
    typename Task<Func, Args...>::future_t run(Func&& func, Args&&... args);

    template<typename Func, typename... Args>
    void parallel_range(size_t range, Func&& func, Args&&... args);
};

template<typename Func, typename... Args>
typename Task<Func, Args...>::future_t ThreadUtils::run(Task<Func, Args...>* task) {
    return ThreadPool::instance()->pushTask(task);
}

template<typename Func, typename... Args>
typename Task<Func, Args...>::future_t ThreadUtils::run(Func&& func, Args&&... args) {
    auto* task = new Task<Func, Args...>(std::forward<Func>(func), std::forward<Args>(args)...);
    return ThreadPool::instance()->pushTask(task);
}

template<typename Func, typename... Args>
void ThreadUtils::parallel_range(size_t range, Func&& func, Args&&... args) {
    size_t threadCount = ThreadPool::instance()->getThreadCount();
    size_t rangePerThread = INT_DIV_CEIL(range, threadCount);

    using future_t = typename Task<Func, size_t, size_t, Args...>::future_t;

    std::vector<future_t> results;
    results.reserve(threadCount);

//    ThreadUtils::batchTask();

    for (size_t i = 0; i < threadCount; ++i) {
        size_t rangeStart = i * rangePerThread;
        size_t rangeEnd = rangeStart + rangePerThread;
        if (rangeEnd >= range)
            rangeEnd = range;
        if (rangeStart == rangeEnd)
            break;

        results.emplace_back(std::move(
                ThreadUtils::run(func, rangeStart, rangeEnd, std::forward<Args>(args)...)
                ));
    }

//    ThreadUtils::endBatch();

    for (size_t i = 0; i < results.size(); ++i)
        results[i].wait();
}

#endif //WORLDENGINE_THREADUTILS_H
