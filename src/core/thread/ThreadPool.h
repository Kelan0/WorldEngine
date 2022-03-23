
#ifndef WORLDENGINE_THREADPOOL_H
#define WORLDENGINE_THREADPOOL_H

#include "core/core.h"
#include "core/thread/WorkerThread.h"

class ThreadPool {
public:
    ThreadPool(size_t concurrency);

    ThreadPool();

    ~ThreadPool();

    template<typename Func, typename... Args>
    std::future<invoke_result_t<Func, Args...>> run(Task<Func, Args...>* task);

    template<typename Func, typename... Args>
    std::future<invoke_result_t<Func, Args...>> run(Func&& func, Args&&... args);

    void batchTasks(size_t batchSizeLimit = SIZE_MAX);

    void endBatch();

    size_t getThreadCount() const;

    size_t getTaskCount() const;

    bool isEmpty() const;

    void waitIdle();

private:
    std::vector<WorkerThread*> m_threads;
    std::shared_ptr<TaskProvider> m_taskProvider;
};

template<typename Func, typename... Args>
std::future<invoke_result_t<Func, Args...>> ThreadPool::run(Task<Func, Args...>* task) {
    return m_taskProvider->pushTask(task);
}

template<typename Func, typename... Args>
std::future<invoke_result_t<Func, Args...>> ThreadPool::run(Func&& func, Args&&... args) {
    return m_taskProvider->pushTask(std::forward<Func>(func), std::forward<Args>(args)...);
}

#endif //WORLDENGINE_THREADPOOL_H
