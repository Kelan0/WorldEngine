
#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t concurrency) {
    m_taskProvider = std::make_shared<TaskProvider>();
    m_threads.reserve(concurrency);
    for (int i = 0; i < concurrency; ++i) {
        m_threads.push_back(new WorkerThread(m_taskProvider));
    }
}

ThreadPool::ThreadPool():
    ThreadPool(std::thread::hardware_concurrency()) {
}

ThreadPool::~ThreadPool() {
    for (size_t i = 0; i < m_threads.size(); ++i)
        delete m_threads[i];
    m_taskProvider.reset();
}

size_t ThreadPool::getThreadCount() const {
    return m_threads.size();
}

size_t ThreadPool::getTaskCount() const {
    return m_taskProvider->getTaskCount();
}

bool ThreadPool::isEmpty() const {
    return m_taskProvider->isEmpty();
}

void ThreadPool::waitIdle() {
    return m_taskProvider->waitIdle();
}

void ThreadPool::batchTasks(size_t batchSizeLimit) {
    m_taskProvider->batchTasks(batchSizeLimit);
}

void ThreadPool::endBatch() {
    m_taskProvider->endBatch();
}
