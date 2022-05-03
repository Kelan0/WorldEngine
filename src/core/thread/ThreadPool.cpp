
#include "ThreadPool.h"
#include "core/util/Profiler.h"


std::unordered_map<std::thread::id, std::atomic_size_t> ThreadPool::s_maxTaskSizes;


ThreadPool::ThreadPool(size_t concurrency) {
    m_threads.resize(concurrency, nullptr);

    m_pushThreadIndex = 0;
    m_taskCount = 0;

    m_isBatchingTasks = false;

    for (size_t i = 0; i < concurrency; ++i) {
        m_threads[i] = new Thread();
        m_threads[i]->running = true;
        m_threads[i]->thread = std::thread(&ThreadPool::executor, this);
        m_threads[i]->initialized = true;
    }
}

ThreadPool::~ThreadPool() {
    for (size_t i = 0; i < m_threads.size(); ++i)
        m_threads[i]->running = false;

    wakeThreads();

    for (size_t i = 0; i < m_threads.size(); ++i)
        m_threads[i]->thread.join();
}

ThreadPool* ThreadPool::instance() {
    static ThreadPool* instance = new ThreadPool();
    return instance;
}

size_t ThreadPool::getThreadCount() const {
    return m_threads.size();
}

size_t ThreadPool::getTaskCount() const {
    return m_taskCount + m_batchedTasks.size();
}

BaseTask* ThreadPool::nextTask(Thread* currentThread) {
    PROFILE_SCOPE("ThreadPool::nextTask")


    assert(currentThread != nullptr);
    assert(currentThread->thread.get_id() == std::this_thread::get_id());

    if (!wakeThreadCondition(currentThread)) {
        PROFILE_SCOPE("Wait for tasks")

        std::unique_lock<std::mutex> lock(m_tasksAvailableMutex);
        while (!wakeThreadCondition(currentThread))
            m_tasksAvailableCondition.wait(lock);

        currentThread->forceWake = false;
    }

//    if (m_taskCount == 0)
//        syncBatchedTasks();

    BaseTask* task = nullptr;

    constexpr size_t maxAttempts = 128;

    size_t attempts = 0;
    size_t failedLocks = 0;
    size_t emptyQueues = 0;
    auto t0 = Performance::now();

    std::uniform_int_distribution<size_t> rand(0, m_threads.size() - 1);
    size_t offset = rand(random());

    PROFILE_REGION("Attempt lock & pop")
    while (task == nullptr && attempts < maxAttempts && m_taskCount > 0) {

        ++attempts;

        for (size_t i = 0; i < m_threads.size(); ++i) {
            size_t index = (offset + i) % m_threads.size();

            Thread* thread = m_threads[index];

            if (thread->taskQueue.empty())
                continue;

            {
                std::unique_lock<std::mutex> lock(thread->mutex, std::try_to_lock);
                if (!lock) {
                    ++failedLocks;
                    continue;
                }

                if (thread->taskQueue.empty()) {
                    ++emptyQueues;
                    continue;
                }

                task = thread->taskQueue.front();
                thread->taskQueue.pop_front();
            }

            --m_taskCount;

            break;
        }
    }

    if (task == nullptr) {
        PROFILE_REGION("Yield")
        std::this_thread::yield(); // Give other threads some execution time before immediately retrying.
    }

    return task;
}

void ThreadPool::executor() {
    PROFILE_SCOPE("ThreadPool::executor");

    while (true) {
        for (const auto& thread : m_threads)
            if (thread == nullptr || !thread->initialized)
                continue; // One or more threads are uninitialized
        break; // All threads are initialized
    }

    Thread* thread = getCurrentThread();
    assert(thread != nullptr);

    thread->forceWake = false;

    while (thread->running) {
        PROFILE_SCOPE("ThreadPool::executor/task_loop")
        BaseTask* task = nextTask(thread);

        if (task == nullptr)
            continue;

        task->exec();

        thread->completeTasks.emplace_back(task);
    }
}

size_t ThreadPool::getCurrentThreadIndex() {
    PROFILE_SCOPE("ThreadPool::getCurrentThreadIndex")
    // TODO: optimise this by sorting threads by ID at initialization, then using std::lower_bound to find the current thread
    // O(log N) instead of O(N), improvement would be more significant on highly multi-threaded systems.
    auto id = std::this_thread::get_id();
    for (size_t i = 0; i < m_threads.size(); ++i) {
        if (m_threads[i]->thread.get_id() == id)
            return i;
    }
    return SIZE_MAX;
}

ThreadPool::Thread* ThreadPool::getCurrentThread() {
    PROFILE_SCOPE("ThreadPool::getCurrentThread")
    size_t index = getCurrentThreadIndex();
    return index < m_threads.size() ? m_threads[index] : nullptr;
}

std::default_random_engine& ThreadPool::random() {
    struct Seed {
        static size_t get() {
            size_t s = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            std::hash_combine(s, std::this_thread::get_id());
//            printf("RNG seed for thread %llu = %llu\n", std::this_thread::get_id(), s);
            return s;
        }
    };
    static thread_local std::default_random_engine random(Seed::get());
    return random;
}

void ThreadPool::beginBatch() {
    PROFILE_SCOPE("ThreadPool::beginBatch")
    assert(!m_isBatchingTasks);

    m_batchedTasksMutex.lock();
    m_isBatchingTasks = true;
}

void ThreadPool::endBatch() {
    PROFILE_SCOPE("ThreadPool::endBatch")
    assert(m_isBatchingTasks);

    syncBatchedTasks();
    m_isBatchingTasks = false;
    m_batchedTasksMutex.unlock();
}

void ThreadPool::syncBatchedTasks() {
    PROFILE_SCOPE("ThreadPool::syncBatchedTasks")

    PROFILE_REGION("Acquire lock")
    std::unique_lock<std::recursive_mutex> batchTasksLock(m_batchedTasksMutex);

    PROFILE_REGION("Distribute tasks to workers")

    size_t numBatchedTasks = m_batchedTasks.size();

    std::uniform_int_distribution<size_t> rand(0, m_threads.size() - 1);
    size_t threadIndex = rand(random());
    size_t tasksPerThread = INT_DIV_CEIL(numBatchedTasks, m_threads.size());

    size_t popIndex = 0;
    while (popIndex < numBatchedTasks) {

        {
            std::unique_lock<std::mutex> threadLock(m_threads[threadIndex]->mutex, std::try_to_lock);

            if (threadLock) {
                for (size_t i = 0; i < tasksPerThread && popIndex < numBatchedTasks; ++i)
                    m_threads[threadIndex]->taskQueue.emplace_back(m_batchedTasks[popIndex++]);
            }
        }

        threadIndex = (threadIndex + 1) % m_threads.size();
    }

    m_batchedTasks.clear();

    {
        PROFILE_REGION("Lock and incr tasks")

        std::unique_lock<std::mutex> taskCountLock(m_tasksAvailableMutex);
        m_taskCount += numBatchedTasks;

        PROFILE_REGION("Notify workers")
        m_tasksAvailableCondition.notify_all();
    }

}

void ThreadPool::flushFrame() {
//    std::set<Thread*> lockedThreads;
//
//    while (lockedThreads.size() < m_threads.size()) {
//        for (size_t i = 0; i < m_threads.size(); ++i) {
//            if (!lockedThreads.contains(m_threads[i])) {
//                if (m_threads[i]->mutex.try_lock())
//                    lockedThreads.emplace(m_threads[i]);
//            }
//        }
//    }
//
//    for (size_t i = 0; i < m_threads.size(); ++i) {
//        auto& completeTasks = m_threads[i]->completeTasks;
//    }
//
//    for (auto it = lockedThreads.rbegin(); it != lockedThreads.rend(); ++it) {
//        Thread* thread = *it;
//        thread->mutex.unlock();
//    }
}

bool ThreadPool::wakeThreadCondition(Thread* thread) const {
    return
//    thread->forceWake ||
    m_taskCount > 0;
}

void ThreadPool::wakeThreads() {
//    PROFILE_SCOPE("ThreadPool::wakeThreads");
//
//    std::unique_lock<std::mutex> lock(m_tasksAvailableMutex);
//    for (size_t i = 0; i < m_threads.size(); ++i)
//        m_threads[i]->forceWake = true;
//
//    m_tasksAvailableCondition.notify_all();
}
