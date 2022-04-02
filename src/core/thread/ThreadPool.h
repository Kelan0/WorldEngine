
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
        bool forceWake;
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

    BaseTask* nextTask(Thread* currentThread);

    void wakeThreads();

private:
    size_t getCurrentThreadIndex();

    Thread* getCurrentThread();

    void executor();

    std::default_random_engine& random();

    void syncBatchedTasks();

    bool wakeThreadCondition(Thread* thread) const;

private:
    std::vector<Thread*> m_threads;
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


#endif //WORLDENGINE_THREADPOOL_H
