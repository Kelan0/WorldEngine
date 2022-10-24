
#ifndef WORLDENGINE_EVENTDISPATCHER_H
#define WORLDENGINE_EVENTDISPATCHER_H

#include "core/core.h"
#include "core/util/Profiler.h"
#include "extern/entt/entt/signal/dispatcher.hpp"

class EventDispatcher;

struct EventDispatcherDestroyedEvent {
    EventDispatcher* eventDispatcher;
};

class TimerId {
    friend class EventDispatcher;
private:
    struct Tracker {
        bool valid;
        uint32_t refCount;
    };
public:
    TimerId();

    TimerId(TimerId& copy);

    TimerId(TimerId&& move) noexcept;

    TimerId(std::nullptr_t);

    ~TimerId();

    TimerId& operator=(const TimerId& copy);

    TimerId& operator=(TimerId&& move) noexcept;

    TimerId& operator=(std::nullptr_t);

    operator bool() const;
    bool operator==(const TimerId& timerId) const;
    bool operator!=(const TimerId& timerId) const;
    size_t hash() const;

private:
    static TimerId get();

    void invalidate();

    void decrRef();

private:
    Tracker* m_tracker;
    uint64_t m_id;
    static uint64_t s_nextId;
};

namespace std {
    template<>
    struct hash<TimerId> {
        size_t operator()(const TimerId& v) const {
            return v.hash();
        }
    };
};

struct TimeoutEvent {
//    typedef void(*Callback)(TimeoutEvent*);
    typedef std::function<void(TimeoutEvent*)> Callback;

    EventDispatcher* eventDispatcher;
    Performance::moment_t startTime;
    Performance::moment_t endTime;
    Callback callback;
    TimerId id;
};

struct IntervalEvent {
//    typedef void(*Callback)(IntervalEvent*);
    typedef std::function<void(IntervalEvent*)> Callback;
    EventDispatcher* eventDispatcher;
    Performance::moment_t startTime;
    Performance::moment_t lastTime;
    Performance::duration_t duration;
    double partialTicks;
    Callback callback;
    TimerId id;
};


template<class Event>
class CallbackWrapper {
    friend class EventDispatcher;
private:
public:
    typedef std::function<void(Event*)> Callback;

public:
    CallbackWrapper(Callback callback);

    CallbackWrapper(const CallbackWrapper& copy);

    CallbackWrapper(CallbackWrapper&& move) noexcept;

    ~CallbackWrapper();

    void call(Event* event);

    explicit operator bool() const;

private:

private:
    Callback m_callback;
    bool m_allocatedInternally;
};



class EventDispatcher {
private:

    template<class Event>
    struct Listener {
        EventDispatcher* eventDispatcher;
        bool disconnectNextReceive;
        void(*callback)(Event*);

        void receive(Event& event);

        static size_t hash(void(*callback)(Event*));
    };

    template<class Event, typename T>
    struct InstanceListener {
        EventDispatcher* eventDispatcher;
        bool disconnectNextReceive;
        void(T::* callback)(Event*);
        T* instance;

        void receive(Event& event);

        static size_t hash(void(T::* callback)(Event*), T* instance);
    };

public:
    EventDispatcher();

    ~EventDispatcher();

    void update();

    template<class Event>
    void connect(void(*callback)(Event*), const bool& once = false);

    template<typename Event>
    void connect(const CallbackWrapper<Event>& callback, const bool& once = false);

    template<typename Event, typename T>
    void connect(void(T::* callback)(Event*), T* instance, const bool& once = false);

    template<class Event>
    void disconnect(void(*callback)(Event*));

    template<class Event, typename T>
    void disconnect(void(T::* callback)(Event*), T* instance);

    template<class Event, typename T>
    void disconnect(T* instance);

    template<typename T>
    void disconnect(T* instance);

    template<class Event>
    void trigger(Event* event);

    template<class Event>
    void repeatTo(EventDispatcher* eventDispatcher);

    void repeatAll(EventDispatcher* eventDispatcher);

    template<class Event>
    bool isRepeatingTo(EventDispatcher* eventDispatcher);

    bool isRepeatingAll(EventDispatcher* eventDispatcher);

    TimerId setTimeout(const TimeoutEvent::Callback& callback, const double& durationMilliseconds);

    TimerId setTimeout(const TimeoutEvent::Callback& callback, const Performance::duration_t& duration);

    TimerId setInterval(const IntervalEvent::Callback& callback, const double& durationMilliseconds);

    TimerId setInterval(const IntervalEvent::Callback& callback, const Performance::duration_t& duration);

    bool clearTimeout(TimerId& id);

    bool clearInterval(TimerId& id);

private:
    void onEventDispatcherDestroyed(EventDispatcherDestroyedEvent* event);

private:
    entt::dispatcher m_dispatcher;
    std::unordered_map<std::type_index, std::unordered_map<size_t, void*>> m_eventListeners;
    std::unordered_map<void*, std::unordered_map<std::type_index, std::unordered_set<size_t>>> m_instanceEventBindings;
    std::vector<EventDispatcher*> m_repeatAllDispatchers;
    std::unordered_map<std::type_index, std::vector<EventDispatcher*>> m_repeatEventDispatchers;
    std::vector<TimeoutEvent*> m_timeouts;
    std::unordered_map<TimerId, TimeoutEvent*> m_timeoutIds;
    std::unordered_map<TimerId, IntervalEvent*> m_intervalIds;
    Performance::moment_t m_lastUpdate;
};


template<class Event>
CallbackWrapper<Event>::CallbackWrapper(Callback callback):
    m_callback(callback),
    m_allocatedInternally(false) {
}

template<class Event>
CallbackWrapper<Event>::CallbackWrapper(const CallbackWrapper& copy):
    m_callback(copy.m_callback),
    m_allocatedInternally(false) {
}

template<class Event>
CallbackWrapper<Event>::CallbackWrapper(CallbackWrapper&& move) noexcept:
    m_callback(std::exchange(move.m_callback, {})),
    m_allocatedInternally(std::exchange(move.m_allocatedInternally, false)) {
}

template<class Event>
CallbackWrapper<Event>::~CallbackWrapper() {
}

template<class Event>
void CallbackWrapper<Event>::call(Event* event) {
    m_callback(event);
}

template<class Event>
CallbackWrapper<Event>::operator bool() const {
    return (bool)m_callback;
}



template<class Event>
inline void EventDispatcher::connect(void(*callback)(Event*), const bool& once) {
    PROFILE_SCOPE("EventDispatcher::connect")
    if (callback == nullptr)
        return;

    auto& listeners = m_eventListeners[std::type_index(typeid(Event))];

    size_t key = Listener<Event>::hash(callback);
    if (listeners.find(key) != listeners.end())
        return; // This key was already inserted, so don't connect the callback a second time

    Listener<Event>* listener = new Listener<Event>();
    listener->eventDispatcher = this;
    listener->disconnectNextReceive = once;
    listener->callback = callback;
    m_dispatcher.sink<Event>().template connect<&Listener<Event>::receive>(*listener);
    listeners.insert(std::make_pair(key, static_cast<void*>(listener)));
}

template<typename Event>
void EventDispatcher::connect(const CallbackWrapper<Event>& callback, const bool& once) {
    PROFILE_SCOPE("EventDispatcher::connect")
    if (!callback)
        return;

    CallbackWrapper<Event>* instance = new CallbackWrapper<Event>(callback);
    instance->m_allocatedInternally = true;
    connect(&CallbackWrapper<Event>::call, instance, once);
}

template<typename Event, typename T>
inline void EventDispatcher::connect(void(T::* callback)(Event*), T* instance, const bool& once) {
    PROFILE_SCOPE("EventDispatcher::connect")
    if (instance == nullptr)
        return;

    if (callback == nullptr)
        return;

    auto& listeners = m_eventListeners[std::type_index(typeid(Event))];

    size_t key = InstanceListener<Event, T>::hash(callback, instance);
    auto it = listeners.find(key);
    if (it != listeners.end())
        return; // This key was already inserted, so don't connect the callback a second time

    InstanceListener<Event, T>* listener = new InstanceListener<Event, T>();
    listener->eventDispatcher = this;
    listener->disconnectNextReceive = once;
    listener->callback = callback;
    listener->instance = instance;
    m_dispatcher.sink<Event>().template connect<&InstanceListener<Event, T>::receive>(*listener);
    listeners.insert(std::make_pair(key, static_cast<void*>(listener)));

    auto& eventBindings = m_instanceEventBindings[static_cast<void*>(instance)];
    auto& bindings = eventBindings[std::type_index(typeid(Event))];
    bindings.insert(key);
}

template<class Event>
inline void EventDispatcher::disconnect(void(*callback)(Event*)) {
    PROFILE_SCOPE("EventDispatcher::disconnect")
    auto it = m_eventListeners.find(std::type_index(typeid(Event)));
    if (it == m_eventListeners.end())
        return;

    auto& listeners = it->second;
    size_t key = Listener<Event>::hash(callback);
    auto it1 = listeners.find(key);
    if (it1 == listeners.end())
        return; // Already disconnected, or was never connected in the first place

    Listener<Event>* listener = static_cast<Listener<Event>*>(it1->second);
    listeners.erase(it1);

    if (listener == nullptr)
        return; // Error?

    m_dispatcher.sink<Event>().template disconnect<&Listener<Event>::receive>(*listener);
    delete listener;

}

template<class Event, typename T>
inline void EventDispatcher::disconnect(void(T::* callback)(Event*), T* instance) {
    PROFILE_SCOPE("EventDispatcher::disconnect")
    auto it = m_eventListeners.find(std::type_index(typeid(Event)));
    if (it == m_eventListeners.end())
        return; // No listeners bound for this event

    auto& listeners = it->second;
    size_t key = InstanceListener<Event, T>::hash(callback, instance);
    auto it1 = listeners.find(key);
    if (it1 == listeners.end())
        return; // Already disconnected, or was never connected in the first place

    InstanceListener<Event, T>* listener = static_cast<InstanceListener<Event, T>*>(it1->second);
    listeners.erase(it1);

    auto it2 = m_instanceEventBindings.find(static_cast<void*>(instance));
    if (it2 != m_instanceEventBindings.end()) {
        auto& eventBindings = it2->second;
        auto it3 = eventBindings.find(std::type_index(typeid(Event)));
        if (it3 != eventBindings.end()) {
            auto& bindings = it3->second;
            bindings.erase(key);
            if (bindings.empty()) {
                eventBindings.erase(it3);
            }
        }
    }

    if constexpr (std::is_same_v<CallbackWrapper<Event>, T>) { // should we use is_convertible_v ?
        CallbackWrapper<Event>* callbackWrapper = static_cast<CallbackWrapper<Event>*>(instance);
        if (callbackWrapper != nullptr && callbackWrapper->m_allocatedInternally) {
            // This is stinky code, however there shouldn't be a way to get external access to an internally
            // allocated CallbackWrapper, therefor I am okay with deleting an object passed to the function
            // without the caller having a way to know, since that should also only be happening internally.
            delete callbackWrapper;
        }
    }

    if (listener == nullptr)
        return;

    m_dispatcher.sink<Event>().template disconnect<&InstanceListener<Event, T>::receive>(*listener);
    delete listener;
}

template<class Event, typename T>
inline void EventDispatcher::disconnect(T* instance) {
    PROFILE_SCOPE("EventDispatcher::disconnect")
    auto it = m_eventListeners.find(std::type_index(typeid(Event)));
    if (it == m_eventListeners.end())
        return; // No listeners bound for this event

    auto& listeners = it->second;

    auto it1 = m_instanceEventBindings.find(static_cast<void*>(instance));
    if (it1 == m_instanceEventBindings.end())
        return; // No instance bindings for this event

    auto& eventBindings = it1->second;
    auto it2 = eventBindings.find(std::type_index(typeid(Event)));
    if (it2 == eventBindings.end())
        return; // No bindings for this instance

    auto& bindings = it2->second;

    for (auto it3 = bindings.begin(); it3 != bindings.end(); ++it3) {
        auto& key = *it3;

        auto it4 = listeners.find(key);
        assert(it4 != listeners.end() && it4->second != nullptr);

        InstanceListener<Event, T>* listener = static_cast<InstanceListener<Event, T>*>(it4->second);
        assert(listener->instance == instance);

        m_dispatcher.sink<Event>().template disconnect<&InstanceListener<Event, T>::receive>(*listener);
        delete listener;
    }

    eventBindings.erase(it2);
}

template<class Event>
inline void EventDispatcher::trigger(Event* event) {
    PROFILE_SCOPE("EventDispatcher::trigger")
    m_dispatcher.trigger(*event);

    auto& dispatchers = m_repeatEventDispatchers[std::type_index(typeid(Event))];
    for (auto it = dispatchers.begin(); it != dispatchers.end(); ++it) {
        (*it)->trigger<Event>(event);
    }

    for (auto it = m_repeatAllDispatchers.begin(); it != m_repeatAllDispatchers.end(); ++it) {
        (*it)->trigger<Event>(event);
    }
}

template<class Event>
inline void EventDispatcher::repeatTo(EventDispatcher* eventDispatcher) {
    PROFILE_SCOPE("EventDispatcher::repeatTo")
    if (eventDispatcher == nullptr)
        return;

    if (isRepeatingTo<Event>(eventDispatcher))
        return;

    auto& dispatchers = m_repeatEventDispatchers[std::type_index(typeid(Event))];
    dispatchers.emplace_back(eventDispatcher);
    eventDispatcher->connect<EventDispatcherDestroyedEvent>(&EventDispatcher::onEventDispatcherDestroyed, this);
}

template<class Event>
inline bool EventDispatcher::isRepeatingTo(EventDispatcher* eventDispatcher) {
    PROFILE_SCOPE("EventDispatcher::isRepeatingTo")
    if (eventDispatcher == nullptr)
        return false;

    if (isRepeatingAll(eventDispatcher))
        return true;

    auto& dispatchers = m_repeatEventDispatchers[std::type_index(typeid(Event))];

    for (auto it = dispatchers.begin(); it != dispatchers.end(); ++it) {
        if ((*it) == eventDispatcher)
            return true;
    }

    return false;
}






template<class Event>
inline void EventDispatcher::Listener<Event>::receive(Event& event) {
    reinterpret_cast<void(*)(Event*)>(callback)(&event);
    if (disconnectNextReceive) {
        eventDispatcher->disconnect(callback);
    }
}

template<class Event>
inline size_t EventDispatcher::Listener<Event>::hash(void(*callback)(Event*)) {
    size_t s = 0;
    std::hash_combine(s, reinterpret_cast<void*>(callback));
    return s;
}

template<class Event, typename T>
inline void EventDispatcher::InstanceListener<Event, T>::receive(Event& event) {
    (instance->*callback)(&event);
    if (disconnectNextReceive) {
        eventDispatcher->disconnect(callback, instance);
    }
}

template<class Event, typename T>
inline size_t EventDispatcher::InstanceListener<Event, T>::hash(void(T::*callback)(Event*), T* instance) {

    size_t s = 0;

    size_t numBytes = sizeof(callback);

    // TODO: is this valid? We need a way to uniquely identify callback, so we reinterpret it as a void* and hash that.
    union {
        void(T::* funcPtr)(Event*);
        void* ptr;
    };
    funcPtr = callback;

    std::hash_combine(s, ptr);
    std::hash_combine(s, instance);
    return s;
}

#endif //WORLDENGINE_EVENTDispatcher_H
