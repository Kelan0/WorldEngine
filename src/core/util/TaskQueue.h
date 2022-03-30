
#ifndef WORLDENGINE_TASKQUEUE_H
#define WORLDENGINE_TASKQUEUE_H

#include "core/core.h"
#include "core/thread/Task.h"
#include <queue>

//class TaskQueue {
//    template<typename Func, typename... Args>
//    using future_t = typename Task<Func, Args...>::future_t;
//public:
//    TaskQueue();
//
//    ~TaskQueue();
//
//    template<typename Func, typename... Args>
//    future_t<Func, Args...> push(Task<Func, Args...>* task);
//
//    template<typename Func, typename... Args>
//    std::optional<future_t<Func, Args...>> tryPush(Task<Func, Args...>* task);
//
//    template<typename Func, typename... Args>
//    future_t<Func, Args...> pop(Task<Func, Args...>* task);
//
//    template<typename Func, typename... Args>
//    std::optional<future_t<Func, Args...>> tryPop(Task<Func, Args...>* task);
//
//
//private:
//    std::deque<BaseTask*> m_tasks;
//    std::mutex m_mutex;
//};


#endif //WORLDENGINE_TASKQUEUE_H
