
#ifndef WORLDENGINE_WORKERTHREAD_H
#define WORLDENGINE_WORKERTHREAD_H

#include "core/core.h"
#include "core/util/Profiler.h"
#include <condition_variable>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <deque>


template<typename Func, typename... Args>
using invoke_result_t = std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;

class WorkerThread;
class TaskProvider;

class BaseTask {
    friend class WorkerThread;
    friend class TaskProvider;
public:
    virtual ~BaseTask() = default;

    virtual void exec() = 0;

private:
    void* m_owner = nullptr;
};


template<typename Func, typename... Args>
class Task : public BaseTask {
    friend class WorkerThread;
    typedef invoke_result_t<Func, Args...> return_t;
public:
    explicit Task(Func&& func, Args... args);

    ~Task() override;

    void exec() override;

    std::future<return_t> getFuture();

private:
    Func m_func;
    std::tuple<Args...> m_args;
    std::promise<return_t> m_promise;
};




class TaskProvider {
    friend class WorkerThread;
public:
    TaskProvider();

    ~TaskProvider();

    std::mutex& mutex();

    void batchTasks(size_t batchSizeLimit = SIZE_MAX);

    void endBatch();

    template<typename Func, typename... Args>
    std::future<invoke_result_t<Func, Args...>> pushTask(Task<Func, Args...>* task);

    template<typename Func, typename... Args>
    std::future<invoke_result_t<Func, Args...>> pushTask(Func&& func, Args&&... args);

    void notifyAll();

    size_t getTaskCount() const;

    bool isEmpty() const;

    void waitIdle();

private:
    BaseTask* nextTask(bool& condition);

    void executeTask(BaseTask* task);

    void syncTasks();

private:
    std::mutex m_activeMutex;
    std::recursive_mutex m_unsyncedMutex;
    std::condition_variable m_nextTaskCondition;
    std::condition_variable m_idleCondition;
    std::deque<BaseTask*> m_activeTasks;
    std::vector<BaseTask*> m_unsyncedTasks;

    size_t m_batchSizeLimit;
    size_t m_processingTasks;
    bool m_idle;
    Profiler::moment_t m_batchStart;
};



class WorkerThread {
    NO_COPY(WorkerThread);
    NO_MOVE(WorkerThread);

public:
    WorkerThread();

    WorkerThread(const std::weak_ptr<TaskProvider>& taskProvider);

    ~WorkerThread();

    template<typename Func, typename... Args>
    std::future<invoke_result_t<Func, Args...>> run(Task<Func, Args...>* task);

    template<typename Func, typename... Args>
    std::future<invoke_result_t<Func, Args...>> run(Func&& func, Args&&... args);

    size_t getTaskCount() const;

    bool idle() const;

    void waitIdle();

private:
    void executor();

private:
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_idleCondition;
    bool m_running;
    bool m_idle;

    std::shared_ptr<TaskProvider> m_taskProvider;
};




template<typename Func, typename... Args>
std::future<invoke_result_t<Func, Args...>> TaskProvider::pushTask(Task<Func, Args...>* task) {

//    Profiler pf; double msec;

    auto ret = task->getFuture();

//    printf("[Thread %llu] TaskProvider::pushTask1 - requested thread lock on m_unsyncedMutex\n", std::this_thread::get_id());
//    pf.mark();

    {
        std::unique_lock<std::recursive_mutex> lock(m_unsyncedMutex);
        m_unsyncedTasks.push_back(task);
        m_idle = false;
    }
//    msec = pf.markMsec();
//    printf("[Thread %llu] TaskProvider::pushTask1 - Took %.5f msec to acquire thread lock on m_unsyncedMutex and push task\n", std::this_thread::get_id(), msec);

    if (m_batchSizeLimit != 0 && m_unsyncedTasks.size() > m_batchSizeLimit) {
//        printf("[Thread %llu] TaskProvider::pushTask1 - Tasks exceeded batchSizeLimit of %llu\n", std::this_thread::get_id(), m_batchSizeLimit);
        syncTasks();
        m_nextTaskCondition.notify_all();
    }

    if (m_batchSizeLimit == 0) {
//        pf.mark();

        m_nextTaskCondition.notify_one();

//        msec = pf.markMsec();
//        printf("[Thread %llu] TaskProvider::pushTask1 - Took %.5f msec to notify task available\n", std::this_thread::get_id(), msec);
    }
    return ret;
}

template<typename Func, typename... Args>
std::future<invoke_result_t<Func, Args...>> TaskProvider::pushTask(Func&& func, Args&&... args) {
//    Profiler pf; double msec;
//    pf.mark();

    auto* task = new Task<Func, std::decay_t<Args>...>(std::forward<Func>(func), std::forward<Args>(args)...);
    task->m_owner = this;

//    msec = pf.markMsec();
//    printf("[Thread %llu] TaskProvider::pushTask2 - Took %.5f msec to create task\n", std::this_thread::get_id(), msec);

    auto ret = pushTask(task);

//    msec = pf.markMsec();
//    printf("[Thread %llu] TaskProvider::pushTask2 - Took %.5f msec to send task\n", std::this_thread::get_id(), msec);

    return ret;
}




template<typename Func, typename... Args>
std::future<invoke_result_t<Func, Args...>> WorkerThread::run(Task<Func, Args...>* task) {
    return m_taskProvider->pushTask(task);
}

template<typename Func, typename... Args>
std::future<invoke_result_t<Func, Args...>> WorkerThread::run(Func&& func, Args&&... args) {
    return m_taskProvider->pushTask(std::forward<Func>(func), std::forward<Args>(args)...);
}




template<typename Func, typename... Args>
Task<Func, Args...>::Task(Func&& func, Args... args):
    m_func(std::forward<Func>(func)),
    m_args(std::forward<Args>(args)...) {
}


template<typename Func, typename... Args>
Task<Func, Args...>::~Task() {

}

template<typename Func, typename... Args>
void Task<Func, Args...>::exec() {

    if constexpr (std::is_same<return_t, void>::value) {
        std::apply(m_func, m_args);
        m_promise.set_value();
    } else {
        return_t ret = std::apply(m_func, m_args);
        m_promise.set_value(std::move(ret));
    }
}

template<typename Func, typename... Args>
std::future<typename Task<Func, Args...>::return_t> Task<Func, Args...>::getFuture() {
    return m_promise.get_future();
}

#endif //WORLDENGINE_WORKERTHREAD_H
