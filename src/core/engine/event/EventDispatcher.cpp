#include "EventDispatcher.h"

EventDispatcher::EventDispatcher() {
}

EventDispatcher::~EventDispatcher() {
    EventDispatcherDestroyedEvent event{};
    event.eventDispatcher = this;
    trigger(&event);

    for (auto it = m_repeatAllDispatchers.begin(); it != m_repeatAllDispatchers.end(); ++it) {
        (*it)->disconnect<EventDispatcherDestroyedEvent>(&EventDispatcher::onEventDispatcherDestroyed, this);
    }

    for (auto it = m_repeatEventDispatchers.begin(); it != m_repeatEventDispatchers.end(); ++it) {
        auto& dispatchers = it->second;

        for (auto it1 = dispatchers.begin(); it1 != dispatchers.end(); ++it1) {
            (*it1)->disconnect<EventDispatcherDestroyedEvent>(&EventDispatcher::onEventDispatcherDestroyed, this);
        }
    }
}

void EventDispatcher::repeatAll(EventDispatcher* eventDispatcher) {
    PROFILE_SCOPE("EntityHierarchy::repeatAll")
    // TODO: prevent circular references, where A repeats to B, then B repeats to A
    if (eventDispatcher == nullptr)
        return;
    if (isRepeatingAll(eventDispatcher))
        return;

    // We are repeating every event to eventDispatcher, so remove any repeat instances bound to individual events.
    for (auto it = m_repeatEventDispatchers.begin(); it != m_repeatEventDispatchers.end(); ++it) {
        auto& dispatchers = it->second;

        for (auto it1 = dispatchers.begin(); it1 != dispatchers.end();) {
            if ((*it1) == eventDispatcher) {
                it1 = dispatchers.erase(it1);
            } else {
                ++it1;
            }
        }
    }

    m_repeatAllDispatchers.emplace_back(eventDispatcher);
    eventDispatcher->connect<EventDispatcherDestroyedEvent>(&EventDispatcher::onEventDispatcherDestroyed, this);
}

bool EventDispatcher::isRepeatingAll(EventDispatcher* eventDispatcher) {
    PROFILE_SCOPE("EntityHierarchy::isRepeatingAll")
    if (eventDispatcher == nullptr)
        return false;

    for (auto it = m_repeatAllDispatchers.begin(); it != m_repeatAllDispatchers.end(); ++it) {
        if ((*it) == eventDispatcher)
            return true;
    }

    return false;
}

void EventDispatcher::onEventDispatcherDestroyed(EventDispatcherDestroyedEvent* event) {
    PROFILE_SCOPE("EntityHierarchy::onEventDispatcherDestroyed")
    for (auto it = m_repeatAllDispatchers.begin(); it != m_repeatAllDispatchers.end();) {
        if ((*it) == event->eventDispatcher) {
            it = m_repeatAllDispatchers.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_repeatEventDispatchers.begin(); it != m_repeatEventDispatchers.end(); ++it) {
        auto& dispatchers = it->second;

        for (auto it1 = dispatchers.begin(); it1 != dispatchers.end();) {
            if ((*it1) == event->eventDispatcher) {
                it1 = dispatchers.erase(it1);
            } else {
                ++it1;
            }
        }
    }
}
