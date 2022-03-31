
#include "ThreadUtils.h"

//ThreadPool* ThreadUtils::POOL = new ThreadPool();

void ThreadUtils::beginBatch() {
    ThreadPool::instance()->beginBatch();
}

void ThreadUtils::endBatch() {
    ThreadPool::instance()->endBatch();
}
