
#ifndef WORLDENGINE_EVENTDISPACHER_H
#define WORLDENGINE_EVENTDISPACHER_H

#include "core/core.h"
#include <entt/signal/dispatcher.hpp>

class EventDispacher;

struct EventDispacherDestroyedEvent {
    EventDispacher* eventDispacher;
};

class EventDispacher {
private:

    template<class Event>
    struct Listener {
        void(*callback)(const Event&);

        void receive(const Event& event);

        static size_t hash(void(*callback)(const Event&));
    };

    template<class Event, typename T>
    struct InstanceListener {
        T* instance;
        void(T::* callback)(const Event&);

        void receive(const Event& event);

        static size_t hash(void(T::* callback)(const Event&), T* instance);
    };

public:
    EventDispacher();

    ~EventDispacher();

    template<class Event>
    void connect(void(*callback)(const Event&));

    template<typename Event, typename T>
    void connect(void(T::* callback)(const Event&), T* instance);

    template<class Event>
    void disconnect(void(*callback)(const Event&));

    template<class Event, typename T>
    void disconnect(void(T::* callback)(const Event&), T* instance);

    template<class Event, typename T>
    void disconnect(T* instance);

    template<typename T>
    void disconnect(T* instance);

    template<class Event>
    void trigger(const Event& event);

    template<class Event>
    void repeatTo(EventDispacher* eventDispacher);

    void repeatAll(EventDispacher* eventDispacher);

    template<class Event>
    bool isRepeatingTo(EventDispacher* eventDispacher);

    bool isRepeatingAll(EventDispacher* eventDispacher);

private:
    void onEventDispacherDestroyed(const EventDispacherDestroyedEvent& event);

private:
    entt::dispatcher m_dispacher;
    std::unordered_map<std::type_index, std::unordered_map<size_t, void*>> m_eventListeners;
    std::unordered_map<void*, std::unordered_map<std::type_index, std::unordered_set<size_t>>> m_instanceEventBindings;
    std::vector<EventDispacher*> m_repeatAllDispachers;
    std::unordered_map<std::type_index, std::vector<EventDispacher*>> m_repeatEventDispachers;
};

template<class Event>
inline void EventDispacher::connect(void(*callback)(const Event&)) {
    PROFILE_SCOPE("EntityHierarchy::connect")
    if (callback == NULL)
        return;

    auto& listeners = m_eventListeners[std::type_index(typeid(Event))];

    size_t key = Listener<Event>::hash(callback);
    if (listeners.find(key) != listeners.end())
        return; // This key was already inserted, so don't connect the callback a second time

    Listener<Event>* listener = new Listener<Event>();
    listener->callback = callback;
    m_dispacher.sink<Event>().template connect<&Listener<Event>::receive>(*listener);
    listeners.insert(std::make_pair(key, static_cast<void*>(listener)));
}

template<typename Event, typename T>
inline void EventDispacher::connect(void(T::* callback)(const Event&), T* instance) {
    PROFILE_SCOPE("EntityHierarchy::connect")
    if (instance == NULL)
        return;

    if (callback == NULL)
        return;

    auto& listeners = m_eventListeners[std::type_index(typeid(Event))];

    size_t key = InstanceListener<Event, T>::hash(callback, instance);
    auto it = listeners.find(key);
    if (it != listeners.end())
        return; // This key was already inserted, so don't connect the callback a second time

    InstanceListener<Event, T>* listener = new InstanceListener<Event, T>();
    listener->callback = callback;
    listener->instance = instance;
    m_dispacher.sink<Event>().template connect<&InstanceListener<Event, T>::receive>(*listener);
    listeners.insert(std::make_pair(key, static_cast<void*>(listener)));

    auto& eventBindings = m_instanceEventBindings[static_cast<void*>(instance)];
    auto& bindings = eventBindings[std::type_index(typeid(Event))];
    bindings.insert(key);
}

template<class Event>
inline void EventDispacher::disconnect(void(*callback)(const Event&)) {
    PROFILE_SCOPE("EntityHierarchy::disconnect")
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

    if (listener == NULL)
        return; // Error?

    m_dispacher.sink<Event>().template disconnect<&Listener<Event>::receive>(*listener);
    delete listener;

}

template<class Event, typename T>
inline void EventDispacher::disconnect(void(T::* callback)(const Event&), T* instance) {
    PROFILE_SCOPE("EntityHierarchy::disconnect")
    auto it = m_eventListeners.find(std::type_index(typeid(Event)));
    if (it == m_eventListeners.end())
        return;

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

    if (listener == NULL)
        return;

    m_dispacher.sink<Event>().template disconnect<&InstanceListener<Event, T>::receive>(*listener);
    delete listener;
}

template<class Event, typename T>
inline void EventDispacher::disconnect(T* instance) {
    PROFILE_SCOPE("EntityHierarchy::disconnect")
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
        assert(it4 != listeners.end() && it4->second != NULL);

        InstanceListener<Event, T>* listener = static_cast<InstanceListener<Event, T>*>(it4->second);
        assert(listener->instance == instance);

        m_dispacher.sink<Event>().template disconnect<&InstanceListener<Event, T>::receive>(*listener);
        delete listener;
    }

    eventBindings.erase(it2);
}

template<class Event>
inline void EventDispacher::trigger(const Event& event) {
    PROFILE_SCOPE("EntityHierarchy::trigger")
    m_dispacher.trigger<Event>(event);

    auto& dispachers = m_repeatEventDispachers[std::type_index(typeid(Event))];
    for (auto it = dispachers.begin(); it != dispachers.end(); ++it) {
        (*it)->trigger(event);
    }

    for (auto it = m_repeatAllDispachers.begin(); it != m_repeatAllDispachers.end(); ++it) {
        (*it)->trigger(event);
    }
}

template<class Event>
inline void EventDispacher::repeatTo(EventDispacher* eventDispacher) {
    PROFILE_SCOPE("EntityHierarchy::repeatTo")
    if (eventDispacher == NULL)
        return;

    if (isRepeatingTo<Event>(eventDispacher))
        return;

    auto& dispachers = m_repeatEventDispachers[std::type_index(typeid(Event))];
    dispachers.emplace_back(eventDispacher);
    eventDispacher->connect<EventDispacherDestroyedEvent>(&EventDispacher::onEventDispacherDestroyed, this);
}

template<class Event>
inline bool EventDispacher::isRepeatingTo(EventDispacher* eventDispacher) {
    PROFILE_SCOPE("EntityHierarchy::isRepeatingTo")
    if (eventDispacher == NULL)
        return false;

    if (isRepeatingAll(eventDispacher))
        return true;

    auto& dispachers = m_repeatEventDispachers[std::type_index(typeid(Event))];

    for (auto it = dispachers.begin(); it != dispachers.end(); ++it) {
        if ((*it) == eventDispacher)
            return true;
    }

    return false;
}

template<class Event>
inline void EventDispacher::Listener<Event>::receive(const Event& event) {
    reinterpret_cast<void(*)(const Event&)>(callback)(event);
}

template<class Event>
inline size_t EventDispacher::Listener<Event>::hash(void(*callback)(const Event&)) {
    size_t s = 0;
    std::hash_combine(s, reinterpret_cast<void*>(callback));
    return s;
}

template<class Event, typename T>
inline void EventDispacher::InstanceListener<Event, T>::receive(const Event& event) {
    (instance->*callback)(event);
}

template<class Event, typename T>
inline size_t EventDispacher::InstanceListener<Event, T>::hash(void(T::*callback)(const Event&), T* instance) {

    size_t s = 0;

    size_t numBytes = sizeof(callback);

    union {
        void(T::* funcPtr)(const Event&);
        void* ptr;
    };
    funcPtr = callback;

    std::hash_combine(s, ptr);
    std::hash_combine(s, instance);
    return s;
}

#endif //WORLDENGINE_EVENTDISPACHER_H
