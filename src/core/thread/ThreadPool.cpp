
#include "ThreadPool.h"
#include "core/util/Profiler.h"


std::unordered_map<std::thread::id, std::atomic_size_t> ThreadPool::s_maxTaskSizes;


ThreadPool::ThreadPool(size_t concurrency) {
    m_threads.resize(concurrency, nullptr);

    m_pushThreadIndex = 0;
    m_taskCount = 0;

    m_mainThreadId = std::this_thread::get_id();

    for (size_t i = 0; i < concurrency; ++i) {
        m_threads[i] = new Thread();
        m_threads[i]->running = true;
        m_threads[i]->thread = std::thread(&ThreadPool::executor, this);
    }
}

ThreadPool::~ThreadPool() {
    for (size_t i = 0; i < m_threads.size(); ++i) {
        m_threads[i]->running = false;
        m_threads[i]->thread.join();
    }
}

ThreadPool* ThreadPool::instance() {
    static ThreadPool* instance = new ThreadPool();
    return instance;
}

size_t ThreadPool::getThreadCount() const {
    return m_threads.size();
}

//BaseTask* ThreadPool::nextTask() {
//    std::optional<BaseTask*> task;
//
//    Thread* thread = getCurrentThread();
//
//    if (thread != nullptr) {
//        task = thread->taskQueue.pop();
//    }
//
//    if (!task.has_value()) {
////        std::uniform_int_distribution<size_t> rdist(0, m_threads.size() - 1);
////        size_t randStart = rdist(random());
//        size_t randStart = 0;
//        size_t stealThreadIndex;
//        for (size_t i = 0; i < m_threads.size(); ++i) {
//            stealThreadIndex = (randStart + i) % m_threads.size();
//            if (m_threads[stealThreadIndex] == thread)
//                continue; // Don't steal from self.
//
//            task = m_threads[stealThreadIndex]->taskQueue.steal();
//            if (task.has_value())
//                break;
//        }
//
//        if (!task.has_value()) {
//            std::this_thread::yield();
//            return nullptr;
//        }
//    }
//
////    printf("[Thread %llu] Successfully obtained task\n", std::this_thread::get_id());
//    return task.value();
//}

BaseTask* ThreadPool::nextTask() {

    size_t currentThreadIndex = getCurrentThreadIndex();

    Profiler pf; double msec;

    if (m_taskCount == 0) {
        //printf("[Thread %llu] Waiting for task...\n", currentThreadIndex);
        //pf.mark();

        std::unique_lock<std::mutex> lock(m_tasksAvailableMutex);
        while (m_taskCount == 0)
            m_tasksAvailableCondition.wait(lock);

        //msec = pf.markMsec();
        //printf("[Thread %llu] Tasks available after %.4f msec\n", currentThreadIndex, msec);
    }


    BaseTask* task = nullptr;

    constexpr size_t maxAttempts = 128;

    size_t attempts = 0;
    size_t failedLocks = 0;
    size_t emptyQueues = 0;
    auto t0 = Profiler::now();

    std::uniform_int_distribution<size_t> rand(0, m_threads.size() - 1);
    size_t offset = rand(random());

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

            {
                //std::unique_lock<std::mutex> lock(m_tasksAvailableMutex);
                --m_taskCount;
            }

            break;
        }

        //if (task == nullptr && m_taskCount == 0)
        //    printf("[Thread %llu] Task was stolen\n", currentThreadIndex);
    }

    //auto t1 = Profiler::now();
    //if (Profiler::milliseconds(t0, t1) > 1) {
    //    if (task != nullptr) {
    //        //if (attempts > 1 || failedLocks > 0)
    //        printf("[Thread %llu] Took %llu attempts, %llu failed locks, %llu empty queues (%.4f msec) to retrieve next task\n", currentThreadIndex, attempts, failedLocks, emptyQueues, Profiler::milliseconds(t0, t1));
    //    } else {
    //        printf("[Thread %llu] Failed to retrieve task after %llu attempts, %llu failed locks, %llu empty queues (%.4f msec)\n", currentThreadIndex, attempts, failedLocks, emptyQueues, Profiler::milliseconds(t0, t1));
    //    }
    //}

    return task;
}

void ThreadPool::executor() {
    printf("Executing thread %llu\n", std::this_thread::get_id());
    std::deque<BaseTask*> garbageTasks;

    Thread* thread = getCurrentThread();
    assert(thread != nullptr);
    while (thread->running) {
        BaseTask* task = nextTask();

        if (garbageTasks.size() > 10) {
            delete garbageTasks.front();
            garbageTasks.pop_front();
        }

        if (task == nullptr)
            continue;

        task->exec();
        //delete task;
        garbageTasks.emplace_back(task);
    }
}

size_t ThreadPool::getCurrentThreadIndex() {
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

