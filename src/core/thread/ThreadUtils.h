
#ifndef WORLDENGINE_THREADUTILS_H
#define WORLDENGINE_THREADUTILS_H

#include "core/thread/ThreadPool.h"

namespace ThreadUtils {

    template<typename Func, typename... Args>
    using future_t = typename Task<Func, Args...>::future_t;

    template<typename Func, typename... Args>
    ThreadUtils::future_t<Func, Args...> run(Task<Func, Args...>* task);

    template<typename Func, typename... Args>
    ThreadUtils::future_t<Func, Args...> run(Func&& func, Args&&... args);

    template<typename Func, typename... Args>
    auto parallel_range(size_t range, size_t alignment, size_t taskCount, Func&& func, Args&&... args);

    template<typename Func, typename... Args>
    auto parallel_range(size_t range, Func&& func, Args&&... args);

    template<typename T, typename... Ts>
    void wait(const std::future<T>& future, const std::future<Ts>&... futures);

    template<typename T>
    void wait(const std::vector<std::future<T>>& futures);

    template<typename T, typename... Ts>
    auto getResults(const std::future<T>& future, const std::future<Ts>&... futures);

    template<typename T>
    std::vector<T> getResults(const std::vector<std::future<T>>& futures);

    void beginBatch();

    void endBatch();

    void wakeThreads();

    size_t getThreadCount();

    uint64_t getThreadHashedId(const std::thread::id& id);

    uint64_t getCurrentThreadHashedId();
};

template<typename Func, typename... Args>
typename ThreadUtils::future_t<Func, Args...> ThreadUtils::run(Task<Func, Args...>* task) {
    PROFILE_SCOPE("ThreadUtils::run")
    return ThreadPool::instance()->pushTask(task);
}

template<typename Func, typename... Args>
typename ThreadUtils::future_t<Func, Args...> ThreadUtils::run(Func&& func, Args&&... args) {
    PROFILE_SCOPE("ThreadUtils::run")
    auto* task = new Task<Func, Args...>(std::forward<Func>(func), std::forward<Args>(args)...);
    return ThreadPool::instance()->pushTask(task);
}

template<typename Func, typename... Args>
auto ThreadUtils::parallel_range(size_t range, size_t alignment, size_t taskCount, Func&& func, Args&&... args) {
    PROFILE_SCOPE("ThreadUtils::parallel_range")

    if (alignment == 0)
        alignment = 1;

    size_t rangePerTask = INT_DIV_CEIL(range, taskCount);
    rangePerTask = INT_DIV_CEIL(rangePerTask, alignment) * alignment;

    using future_t = ThreadUtils::future_t<Func, size_t, size_t, Args...>;

    std::vector<future_t> results;
    results.reserve(taskCount);

    ThreadUtils::beginBatch();
    for (size_t i = 0; i < taskCount; ++i) {
        PROFILE_SCOPE("Create thread task")

        size_t rangeStart = i * rangePerTask;
        size_t rangeEnd = rangeStart + rangePerTask;
        if (rangeEnd >= range)
            rangeEnd = range;
        if (rangeStart >= rangeEnd)
            break;

        PROFILE_REGION("Dispatch task")
        auto result = std::move(ThreadUtils::run(func, rangeStart, rangeEnd, std::forward<Args>(args)...));

        PROFILE_REGION("Store result")
        results.emplace_back(std::move(result));
    }

    ThreadUtils::endBatch();

//    std::this_thread::yield();

    return std::move(results);
}

template<typename Func, typename... Args>
auto ThreadUtils::parallel_range(size_t range, Func&& func, Args&&... args) {
    return ThreadUtils::parallel_range<Func, Args...>(range, 1, ThreadUtils::getThreadCount(), std::forward<Func>(func), std::forward<Args>(args)...);
}


template<typename T, typename... Ts>
void ThreadUtils::wait(const std::future<T>& future, const std::future<Ts>&... futures) {
    PROFILE_SCOPE("ThreadUtils::wait")
    future.wait();
    if constexpr (sizeof...(futures) != 0)
        ThreadUtils::wait(futures...);
}

template<typename T>
void ThreadUtils::wait(const std::vector<std::future<T>>& futures) {
    PROFILE_SCOPE("ThreadUtils::wait")
    for (auto& future : futures)
        future.wait();
}

template<typename T, typename... Ts>
auto ThreadUtils::getResults(const std::future<T>& future, const std::future<Ts>&... futures) {
    PROFILE_SCOPE("ThreadUtils::getResults")
    if constexpr (sizeof...(futures) == 0) {
        return const_cast<std::future<T>&>(future).get();
    } else {
        return std::make_tuple(const_cast<std::future<T>&>(future).get(), getResults(futures)...);
    }
}

template<typename T>
std::vector<T> ThreadUtils::getResults(const std::vector<std::future<T>>& futures) {
    PROFILE_SCOPE("ThreadUtils::getResults")

    std::vector<T> results;
    results.reserve(futures.size());

    auto& nonConstFutures = const_cast<std::vector<std::future<T>>&>(futures);
    for (auto&& future : nonConstFutures)
        results.emplace_back(future.get());

    return results;
}

#endif //WORLDENGINE_THREADUTILS_H
