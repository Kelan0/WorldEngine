
#include "WorkerThread.h"



TaskProvider::TaskProvider():
    m_processingTasks(0),
    m_batchSizeLimit(0),
    m_idle(true) {
}

TaskProvider::~TaskProvider() {

}

std::mutex& TaskProvider::mutex() {
    return m_activeMutex;
}

void TaskProvider::notifyAll() {
    m_nextTaskCondition.notify_all();
}

size_t TaskProvider::getTaskCount() const {
    return m_activeTasks.size() + m_unsyncedTasks.size();
}

bool TaskProvider::isEmpty() const {
    return m_activeTasks.empty() && m_unsyncedTasks.empty();
}

void TaskProvider::waitIdle() {
    if (m_processingTasks == 0)
        return;

//    Profiler pf; double msec;
//    printf("[Thread %llu] TaskProvider::waitIdle - Requested thread lock on m_activeMutex\n", std::this_thread::get_id());
//    auto t0 = Profiler::now();
//    pf.mark();

    std::unique_lock<std::mutex> lock(m_activeMutex);

//    msec = pf.markMsec();
//    printf("[Thread %llu] TaskProvider::waitIdle - Took %.5f msec to acquire thread lock\n", std::this_thread::get_id(), msec);
//    pf.mark();

    while (m_processingTasks != 0)
        m_idleCondition.wait(lock);

//    msec = pf.markMsec();
//    printf("[Thread %llu] TaskProvider::waitIdle - Took %.5f msec for all tasks to be processed\n", std::this_thread::get_id(), msec);
//    msec = Profiler::milliseconds(t0, Profiler::now());
//    printf("[Thread %llu] TaskProvider::waitIdle - Releasing thread lock on m_activeMutex after %.5f msec\n", std::this_thread::get_id(), msec);

}

BaseTask* TaskProvider::nextTask(bool& condition) {
//    Profiler pf; double msec;
//    printf("[Thread %llu] TaskProvider::nextTask - Requested thread lock on m_activeMutex\n", std::this_thread::get_id());
//    auto t0 = Profiler::now();
//    pf.mark();

    std::unique_lock<std::mutex> lock(m_activeMutex);

//    msec = pf.markMsec();
//    printf("[Thread %llu] TaskProvider::nextTask - Took %.5f msec to acquire thread lock\n", std::this_thread::get_id(), msec);

    if (getTaskCount() == 0) {
//        printf("[Thread %llu] TaskProvider::nextTask - No tasks, waiting...\n", std::this_thread::get_id());
//        pf.mark();

        while (getTaskCount() == 0)
            m_nextTaskCondition.wait(lock);

//        printf("[Thread %llu] TaskProvider::nextTask - Waited %.5f msec for a task to arrive\n", std::this_thread::get_id(), msec);
    }

    BaseTask* task = nullptr;

    if (m_activeTasks.empty())
        syncTasks(); // As soon as we run out of tasks, re-synchronize task list to keep threads busy

    if (!m_activeTasks.empty()) {
        task = m_activeTasks.front();
        m_activeTasks.pop_front();
    }

    if (task != nullptr)
        ++m_processingTasks;

//    msec = Profiler::milliseconds(t0, Profiler::now());
//    printf("[Thread %llu] TaskProvider::nextTask - Releasing thread lock on m_activeMutex after %.5f msec\n", std::this_thread::get_id(), msec);

    return task;
}

void TaskProvider::executeTask(BaseTask* task) {
//    Profiler pf; double msec;

    if (task != nullptr) {
//        pf.mark();
        task->exec();

        if (task->m_owner == this)
            delete task;

//        msec = pf.markMsec();
//        printf("[Thread %llu] TaskProvider::executeTask - Took %.5f msec to execute task\n", std::this_thread::get_id(), msec);

        {
//            printf("[Thread %llu] TaskProvider::executeTask - Requested thread lock on m_activeMutex\n", std::this_thread::get_id());
//            auto t0 = pf.now();
//            pf.mark();

            std::unique_lock<std::mutex> lock(m_activeMutex);

//            msec = pf.markMsec();
//            printf("[Thread %llu] TaskProvider::executeTask - Took %.5f msec to acquire thread lock\n", std::this_thread::get_id(), msec);

            --m_processingTasks;
            if (getTaskCount() == 0 && m_processingTasks == 0) {
                m_idle = true;

//                pf.mark();
//                m_idleCondition.notify_all();
//                msec = pf.markMsec();
//                printf("[Thread %llu] TaskProvider::executeTask - Took %.5f msec to notify_all idleCondition\n", std::this_thread::get_id(), msec);
//                msec = pf.milliseconds(t0, pf.now());
//                printf("[Thread %llu] TaskProvider::executeTask - Releasing thread lock on m_activeMutex after %.5f msec\n", std::this_thread::get_id(), msec);
            }
        }
    }
}

void TaskProvider::syncTasks() {
//    Profiler pf; double msec;

    if (m_unsyncedTasks.empty())
        return;

//    printf("[Thread %llu] TaskProvider::syncTasks - Requested thread lock on m_unsyncedMutex - Synchronizing %d tasks\n", std::this_thread::get_id(), m_unsyncedTasks.size());
//    auto t0 = Profiler::now();
//    pf.mark();

    std::unique_lock<std::recursive_mutex> lock(m_unsyncedMutex);

//    msec = pf.markMsec();
//    printf("[Thread %llu] TaskProvider::syncTasks - Took %.5f msec to acquire thread lock\n", std::this_thread::get_id(), msec);

    for (auto it = m_unsyncedTasks.begin(); it != m_unsyncedTasks.end(); ++it)
        m_activeTasks.emplace_back(*it);

    m_unsyncedTasks.clear();

//    pf.mark();

    m_nextTaskCondition.notify_all();

//    msec = pf.markMsec();
//    printf("[Thread %llu] TaskProvider::syncTasks - Took %.5f msec to notify all threads of available tasks\n", std::this_thread::get_id(), msec);
//    msec = Profiler::milliseconds(t0, Profiler::now());
//    printf("[Thread %llu] TaskProvider::syncTasks - Releasing thread lock on m_unsyncedMutex after %.5f msec\n", std::this_thread::get_id(), msec);
}

void TaskProvider::batchTasks(size_t batchSizeLimit) {
#if _DEBUG
    if (m_batchSizeLimit != 0) {
        printf("TaskProvider::batchTasks - Already started batch\n");
        assert(false);
        return;
    }
#endif
    printf("[Thread %llu] TaskProvider::batchTasks - Start batch, batchSizeLimit = %llu\n", std::this_thread::get_id(), batchSizeLimit);

    m_unsyncedMutex.lock();
    m_batchSizeLimit = batchSizeLimit;
    if (m_batchSizeLimit < m_unsyncedTasks.size())
        syncTasks();

    m_batchStart = Profiler::now();
}

void TaskProvider::endBatch() {
#if _DEBUG
    if (m_batchSizeLimit == 0) {
        printf("TaskProvider::endBatch - No batch started\n");
        assert(false);
        return;
    }
#endif

    double msec = Profiler::milliseconds(Profiler::now() - m_batchStart);
    printf("[Thread %llu] TaskProvider::endBatch - End batch, batchSizeLimit = %llu - Took %.5f msec\n", std::this_thread::get_id(), m_batchSizeLimit, msec);

    m_batchSizeLimit = 0;
    syncTasks();
    m_unsyncedMutex.unlock();
}


WorkerThread::WorkerThread():
    WorkerThread(std::make_shared<TaskProvider>()) {
}

WorkerThread::WorkerThread(const std::weak_ptr<TaskProvider>& taskProvider):
    m_running(true),
    m_idle(true),
    m_taskProvider(taskProvider),
    m_thread(&WorkerThread::executor, this) {
}

WorkerThread::~WorkerThread() {
    printf("[Thread %llu] Destroying WorkerThread\n", std::this_thread::get_id());

    {
        std::unique_lock<std::mutex> lock(m_taskProvider->mutex());
        m_running = false;
    }
    m_taskProvider->notifyAll();

    m_thread.join();
}

void WorkerThread::executor() {

    while (m_running) {

        BaseTask* task = m_taskProvider->nextTask(m_running);
        m_idle = false;
        m_taskProvider->executeTask(task);

        if (m_taskProvider->isEmpty()) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_idle = true;
            m_idleCondition.notify_all();
        }
    }
}

size_t WorkerThread::getTaskCount() const {
    return m_taskProvider->getTaskCount();
}

bool WorkerThread::idle() const {
    return m_idle;
}

void WorkerThread::waitIdle() {
    if (m_idle)
        return;

    std::unique_lock<std::mutex> lock(m_mutex);
    m_idleCondition.wait(lock, [this]() {
        return m_idle;
    });
}
