
#ifndef WORLDENGINE_TASK_H
#define WORLDENGINE_TASK_H

#include "core/core.h"


class BaseTask {
public:
    virtual ~BaseTask() = default;

    virtual void exec() = 0;
};




template<typename Func, typename... Args>
class Task : public BaseTask {

public:
    typedef std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...> return_t;
    typedef std::future<return_t> future_t;

public:
    explicit Task(Func&& func, std::decay_t<Args>... args);

    ~Task() override;

    void exec() override;

    future_t getFuture();

private:
    Func m_func;
    std::tuple<std::decay_t<Args>...> m_args;
    std::promise<return_t> m_promise;
};







template<typename Func, typename... Args>
Task<Func, Args...>::Task(Func&& func, std::decay_t<Args>... args):
        m_func(std::forward<Func>(func)),
        m_args(std::forward<std::decay_t<Args>>(args)...) {
}

template<typename Func, typename... Args>
Task<Func, Args...>::~Task() {

}

template<typename Func, typename... Args>
void Task<Func, Args...>::exec() {
    PROFILE_SCOPE("Task::exec")

    if constexpr (std::is_same<return_t, void>::value) {
        std::apply(m_func, m_args);
        m_promise.set_value();
    } else {
        return_t ret = std::apply(m_func, m_args);
        m_promise.set_value(std::move(ret));
    }
}

template<typename Func, typename... Args>
typename Task<Func, Args...>::future_t Task<Func, Args...>::getFuture() {
    return m_promise.get_future();
}


#endif //WORLDENGINE_TASK_H
