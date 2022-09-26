
#include "ThreadUtils.h"

//ThreadPool* ThreadUtils::POOL = new ThreadPool();

void ThreadUtils::beginBatch() {
    ThreadPool::instance()->beginBatch();
}

void ThreadUtils::endBatch() {
    ThreadPool::instance()->endBatch();
}

void ThreadUtils::wakeThreads() {
    ThreadPool::instance()->wakeThreads();
}

size_t ThreadUtils::getThreadCount() {
    return ThreadPool::instance()->getThreadCount();
}

uint64_t ThreadUtils::getThreadHashedId(const std::thread::id& id) {
    return (uint64_t)std::hash<std::thread::id>{}(id);
}

uint64_t ThreadUtils::getCurrentThreadHashedId() {
    return ThreadUtils::getThreadHashedId(std::this_thread::get_id());
}
