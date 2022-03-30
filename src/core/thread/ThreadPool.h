
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
        bool running;
    };

public:
    ThreadPool(size_t concurrency = std::thread::hardware_concurrency());

    ~ThreadPool();

    static ThreadPool* instance();

    size_t getThreadCount() const;

    template<typename Func, typename... Args>
    future_t<Func, Args...> pushTask(Task<Func, Args...>* task);

    BaseTask* nextTask();

private:
    size_t getCurrentThreadIndex();

    Thread* getCurrentThread();

    void executor();

    std::default_random_engine& random();
private:
    std::vector<Thread*> m_threads;
    std::thread::id m_mainThreadId;
//    wsque<BaseTask*> m_mainThreadTaskQueue;
//    std::mutex m_taskQueueMutex;
    std::atomic_size_t m_pushThreadIndex;

    std::atomic_size_t m_taskCount;
    std::mutex m_tasksAvailableMutex;
    std::condition_variable m_tasksAvailableCondition;
};




template<typename Func, typename... Args>
typename ThreadPool::future_t<Func, Args...> ThreadPool::pushTask(Task<Func, Args...>* task) {
    assert(task != nullptr);

//    Profiler pf; double msec, accum = 0.0;
//    printf("ThreadPool::pushTask - ==== START\n");
//    pf.mark();

    auto future = task->getFuture();

    size_t pushThreadIndex = m_pushThreadIndex++;
    pushThreadIndex = pushThreadIndex % m_threads.size();

//    msec = pf.markMsec();
//    accum += msec;
//    printf("Took %.4f msec to increment pushThreadIndex round robbin\n", msec);
//    pf.mark();

    {
        std::unique_lock<std::mutex> lock(m_threads[pushThreadIndex]->mutex);
        m_threads[pushThreadIndex]->taskQueue.push_back(task);
    }

//    msec = pf.markMsec();
//    accum += msec;
//    printf("ThreadPool::pushTask - Took %.4f msec to push task\n", msec);
//    pf.mark();

    {
        std::unique_lock<std::mutex> lock(m_tasksAvailableMutex);
        ++m_taskCount;
    }

//    msec = pf.markMsec();
//    accum += msec;
//    printf("ThreadPool::pushTask - Took %.4f msec to increment m_taskCount\n", msec);
//    pf.mark();

    m_tasksAvailableCondition.notify_one();

//    msec = pf.markMsec();
//    accum += msec;
//    printf("ThreadPool::pushTask - Took %.4f msec to notify available task\n", msec);
//    printf("ThreadPool::pushTask - ==== END Took %.4f msec\n", accum);

    return future;
}


#endif //WORLDENGINE_THREADPOOL_H
