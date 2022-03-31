
#ifndef WORLDENGINE_THREADPOOL_H
#define WORLDENGINE_THREADPOOL_H

#include "core/core.h"
#include "core/thread/Task.h"
#include "core/util/wsque.hpp"
#include <thread>
#include <mutex>
#include <random>
#include "core/util/Profiler.h"


class ThreadPool {
public:
    static std::unordered_map<std::thread::id, std::atomic_size_t> s_maxTaskSizes;

private:
    template<typename Func, typename... Args>
    using future_t = typename Task<Func, Args...>::future_t;

    struct Thread {
        std::thread thread;
        std::mutex mutex;
        //wsque<BaseTask*> taskQueue;
        std::deque<BaseTask*> taskQueue;
        std::vector<BaseTask*> completeTasks;
        bool running;
    };

public:
    ThreadPool(size_t concurrency = std::thread::hardware_concurrency());

    ~ThreadPool();

    static ThreadPool* instance();

    size_t getThreadCount() const;

    size_t getTaskCount() const;

    void flushFrame();

    template<typename Func, typename... Args>
    future_t<Func, Args...> pushTask(Task<Func, Args...>* task);

//    template<typename Func, typename... Args>
//    void pushTasks(size_t count, Task<Func, Args...>** tasks, future_t<Func, Args...>* futures);

    void beginBatch();

    void endBatch();

    BaseTask* nextTask();

private:
    size_t getCurrentThreadIndex();

    Thread* getCurrentThread();

    void executor();

    std::default_random_engine& random();

    void syncBatchedTasks();

private:
    std::vector<Thread*> m_threads;
    std::thread::id m_mainThreadId;
//    wsque<BaseTask*> m_mainThreadTaskQueue;
//    std::mutex m_taskQueueMutex;
    std::atomic_size_t m_pushThreadIndex;

    std::atomic_size_t m_taskCount;
    std::mutex m_tasksAvailableMutex;
    std::condition_variable m_tasksAvailableCondition;

    std::vector<BaseTask*> m_batchedTasks;
    std::recursive_mutex m_batchedTasksMutex;
    bool m_isBatchingTasks;


};




template<typename Func, typename... Args>
typename ThreadPool::future_t<Func, Args...> ThreadPool::pushTask(Task<Func, Args...>* task) {
    PROFILE_SCOPE("ThreadPool::pushTask")

    assert(task != nullptr);

    auto future = task->getFuture();


    if (m_isBatchingTasks) {
        m_batchedTasks.emplace_back(task);

    } else {
        size_t pushThreadIndex = m_pushThreadIndex++;

        {
            PROFILE_REGION("Lock and push")
            pushThreadIndex = pushThreadIndex % m_threads.size();
            std::unique_lock<std::mutex> lock(m_threads[pushThreadIndex]->mutex);
            m_threads[pushThreadIndex]->taskQueue.push_back(task);
        }

        {
            PROFILE_REGION("Lock and incr tasks")
            std::unique_lock<std::mutex> lock(m_tasksAvailableMutex);
            ++m_taskCount;

            PROFILE_REGION("Notify task available")
            m_tasksAvailableCondition.notify_one();
        }

    }

    return future;
}

//template<typename Func, typename... Args>
//void ThreadPool::pushTasks(size_t count, Task<Func, Args...>** tasks, future_t<Func, Args...>* futures) {
//    PROFILE_SCOPE("ThreadPool::pushTasks")
//
//    size_t pushThreadStartIndex = m_pushThreadIndex.fetch_add(count); // TODO: can we get away with memory_order_relaxed here?
//
//    PROFILE_REGION("Lock and push tasks")
//
//    size_t desiredThreads = std::min(count, m_threads.size());
//
//    constexpr size_t maxLockAttempts = 128;
//    size_t failedLocks = 0;
//    size_t numLocked = 0;
//    std::vector<Thread*> lockedThreads(m_threads.size(), nullptr);
//
//    while (failedLocks < maxLockAttempts && numLocked < desiredThreads) {
//        for (size_t i = 0; i < m_threads.size() && numLocked < desiredThreads; ++i) {
//            if (lockedThreads[i] != nullptr) {
//                continue; // Already locked
//            }
//
//            if (!m_threads[i]->mutex.try_lock()) {
//                ++failedLocks;
//                continue; // Failed to lock
//            }
//
//            lockedThreads[i] = m_threads[i];
//            ++numLocked;
//        }
//    }
//
//    if (numLocked == 0) {
//        size_t pushThreadIndex = pushThreadStartIndex % m_threads.size();
//        m_threads[pushThreadIndex]->mutex.lock();
//        lockedThreads[pushThreadIndex] = m_threads[pushThreadIndex];
//    }
//
//    size_t taskIndex = 0;
//    while (taskIndex < count) {
//        for (size_t i = 0; i < lockedThreads.size(); ++i) {
//            if (lockedThreads[i] != nullptr) {
//                lockedThreads[i]->taskQueue.emplace_back(tasks[taskIndex]);
//                futures[taskIndex] = std::move(tasks[taskIndex]->getFuture());
//                ++taskIndex;
//            }
//        }
//    }
//    for (size_t i = 0; i < lockedThreads.size(); ++i) {
//        if (lockedThreads[i] != nullptr) {
//            lockedThreads[i]->mutex.unlock();
//        }
//    }
//
//    PROFILE_REGION("Lock and incr tasks")
//    {
//        std::unique_lock<std::mutex> lock(m_tasksAvailableMutex);
//        m_taskCount.fetch_add(count);
//    }
//
//    PROFILE_REGION("Notify tasks available")
//    if (count == 1) {
//        m_tasksAvailableCondition.notify_one();
//    } else {
//        m_tasksAvailableCondition.notify_all();
//    }
//}


#endif //WORLDENGINE_THREADPOOL_H
