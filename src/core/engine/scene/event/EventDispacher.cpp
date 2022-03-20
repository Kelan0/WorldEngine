#include "core/engine/scene/event/EventDispacher.h"

EventDispacher::EventDispacher() {
}

EventDispacher::~EventDispacher() {
    EventDispacherDestroyedEvent event;
    event.eventDispacher = this;
    trigger(event);

    for (auto it = m_repeatAllDispachers.begin(); it != m_repeatAllDispachers.end(); ++it) {
        (*it)->disconnect<EventDispacherDestroyedEvent>(&EventDispacher::onEventDispacherDestroyed, this);
    }

    for (auto it = m_repeatEventDispachers.begin(); it != m_repeatEventDispachers.end(); ++it) {
        auto& dispachers = it->second;

        for (auto it1 = dispachers.begin(); it1 != dispachers.end(); ++it1) {
            (*it1)->disconnect<EventDispacherDestroyedEvent>(&EventDispacher::onEventDispacherDestroyed, this);
        }
    }
}

void EventDispacher::repeatAll(EventDispacher* eventDispacher) {
    // TODO: prevent circular references, where A repeats to B, then B repeats to A
    if (eventDispacher == NULL)
        return;
    if (isRepeatingAll(eventDispacher))
        return;

    // We are repeating every event to eventDispacher, so remove any repeat instances bound to individual events.
    for (auto it = m_repeatEventDispachers.begin(); it != m_repeatEventDispachers.end(); ++it) {
        auto& dispachers = it->second;

        for (auto it1 = dispachers.begin(); it1 != dispachers.end();) {
            if ((*it1) == eventDispacher) {
                it1 = dispachers.erase(it1);
            } else {
                ++it1;
            }
        }
    }

    m_repeatAllDispachers.emplace_back(eventDispacher);
    eventDispacher->connect<EventDispacherDestroyedEvent>(&EventDispacher::onEventDispacherDestroyed, this);
}

bool EventDispacher::isRepeatingAll(EventDispacher* eventDispacher) {
    if (eventDispacher == NULL)
        return false;

    for (auto it = m_repeatAllDispachers.begin(); it != m_repeatAllDispachers.end(); ++it) {
        if ((*it) == eventDispacher)
            return true;
    }

    return false;
}

void EventDispacher::onEventDispacherDestroyed(const EventDispacherDestroyedEvent& event) {
    for (auto it = m_repeatAllDispachers.begin(); it != m_repeatAllDispachers.end();) {
        if ((*it) == event.eventDispacher) {
            it = m_repeatAllDispachers.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_repeatEventDispachers.begin(); it != m_repeatEventDispachers.end(); ++it) {
        auto& dispachers = it->second;

        for (auto it1 = dispachers.begin(); it1 != dispachers.end();) {
            if ((*it1) == event.eventDispacher) {
                it1 = dispachers.erase(it1);
            } else {
                ++it1;
            }
        }
    }
}
