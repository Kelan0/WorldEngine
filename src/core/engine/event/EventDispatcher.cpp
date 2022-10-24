#include <utility>

#include "core/engine/event/EventDispatcher.h"
#include "core/thread/ThreadUtils.h"

uint64_t TimerId::s_nextId = 1;

TimerId::TimerId():
    m_id(0),
    m_tracker(nullptr) {
}

TimerId::TimerId(TimerId& copy):
    m_id(copy.m_id),
    m_tracker(copy.m_tracker) {
    if (m_tracker != nullptr)
        ++m_tracker->refCount;
}

TimerId::TimerId(TimerId&& move) noexcept:
    m_id(std::exchange(move.m_id, 0)),
    m_tracker(std::exchange(move.m_tracker, nullptr)) {
}

TimerId::TimerId(std::nullptr_t):
    m_id(0),
    m_tracker(nullptr) {
}

TimerId::~TimerId() {
    decrRef();
}

TimerId& TimerId::operator=(const TimerId& copy) {
    if (this != &copy) {
        m_id = copy.m_id;
        m_tracker = copy.m_tracker;
        if (m_tracker != nullptr)
            ++m_tracker->refCount;
    }
    return *this;
}

TimerId& TimerId::operator=(TimerId&& move) noexcept {
    m_id = std::exchange(move.m_id, 0);
    m_tracker = std::exchange(move.m_tracker, nullptr);
    return *this;
}

TimerId& TimerId::operator=(std::nullptr_t) {
    m_id = 0;
    decrRef();
    m_tracker = nullptr;
    return *this;
}

TimerId::operator bool() const {
    return m_tracker != nullptr && m_tracker->valid && m_id != 0;
}

bool TimerId::operator==(const TimerId& timerId) const {
    return m_id == timerId.m_id;
}

bool TimerId::operator!=(const TimerId& timerId) const {
    return !(*this == timerId);
}

size_t TimerId::hash() const {
    return std::hash<uint64_t>{}(m_id);
}

TimerId TimerId::get() {
    TimerId id{};
    id.m_id = ++s_nextId;
    id.m_tracker = new Tracker();
    id.m_tracker->valid = true;
    id.m_tracker->refCount = 1;
    return id;
}

void TimerId::invalidate() {
    if (m_tracker != nullptr)
        m_tracker->valid = false;
}

void TimerId::decrRef() {
    if (m_tracker != nullptr) {
        --m_tracker->refCount;
        if (m_tracker->refCount == 0) {
            delete m_tracker;
        }
    }
}

EventDispatcher::EventDispatcher():
    m_lastUpdate(Performance::zero_moment) { // lastUpdate is zero nanoseconds since epoch
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

void EventDispatcher::update() {
    Performance::moment_t currentTime = Performance::now();
    if (m_lastUpdate == Performance::zero_moment) {
        m_lastUpdate = currentTime; // First update, sync from epoch to now
    }
    uint64_t elapsedNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - m_lastUpdate).count();

    for (auto& [id, interval] : m_intervalIds) {
        uint64_t intervalDurationNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(interval->duration).count();
        interval->partialTicks += (double)elapsedNanos / (double)intervalDurationNanos;
        if (interval->partialTicks >= 1.0) {
            interval->partialTicks -= 1.0;
            interval->callback(interval);
            interval->lastTime = currentTime;
        }
        if (interval->partialTicks > 10.0) {
            // This update loop is failing to keep up with the interval, forcefully reset it.
            interval->partialTicks = 0.0;
        }
    }

    int64_t eraseCount = 0;

    for (auto it0 = m_timeouts.begin(); it0 != m_timeouts.end(); ++it0, ++eraseCount) {
        TimeoutEvent* timeout = *it0;

        if (timeout->endTime > currentTime) {
            // This timeout and all subsequent ones are still in the future. Stop processing them.
            break;
        }

        timeout->callback(timeout);
        timeout->id.invalidate();

        auto it1 = m_timeoutIds.find(timeout->id);
        if (it1 != m_timeoutIds.end()) {
            m_timeoutIds.erase(it1);
        }
    }

    if (eraseCount > 0) {
        m_timeouts.erase(m_timeouts.begin(), m_timeouts.begin() + eraseCount);
    }

    m_lastUpdate = currentTime;
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

TimerId EventDispatcher::setTimeout(const TimeoutEvent::Callback& callback, const double& durationMilliseconds) {
    return setTimeout(callback, std::chrono::duration_cast<Performance::duration_t>(std::chrono::duration<double, std::milli>(durationMilliseconds)));
}

TimerId EventDispatcher::setTimeout(const TimeoutEvent::Callback& callback, const Performance::duration_t& duration) {

    TimerId id = TimerId::get();
    TimeoutEvent* timeout = new TimeoutEvent();
    timeout->eventDispatcher = this;
    timeout->startTime = Performance::now();
    timeout->endTime = timeout->startTime + duration;
    timeout->callback = callback;
    timeout->id = id;

    m_timeoutIds.insert(std::make_pair(id, timeout));

    auto it = std::upper_bound(m_timeouts.begin(), m_timeouts.end(), timeout->endTime, [](const Performance::moment_t& endTime, const TimeoutEvent* timeoutEvent) {
        return endTime < timeoutEvent->endTime;
    });

    m_timeouts.insert(it, timeout);

    return id;
}

TimerId EventDispatcher::setInterval(const IntervalEvent::Callback& callback, const double& durationMilliseconds) {
    return setInterval(callback, std::chrono::duration_cast<Performance::duration_t>(std::chrono::duration<double, std::milli>(durationMilliseconds)));
}

TimerId EventDispatcher::setInterval(const IntervalEvent::Callback& callback, const Performance::duration_t& duration) {
    TimerId id = TimerId::get();
    IntervalEvent* interval = new IntervalEvent();
    interval->eventDispatcher = this;
    interval->startTime = Performance::now();
    interval->lastTime = interval->startTime;
    interval->partialTicks = 0.0;
    interval->duration = duration;
    interval->callback = callback;
    interval->id = id;

    m_intervalIds.insert(std::make_pair(id, interval));

    return id;
}

bool EventDispatcher::clearTimeout(TimerId& id) {
    if (!id) {
        return true; // "Successfully" cleared a non-existent ID
    }
    auto it0 = m_timeoutIds.find(id);
    if (it0 == m_timeoutIds.end()) {
        return false;
    }

    TimeoutEvent* timeout = it0->second;
    assert(timeout->id == id);

    // Find the lower bound to start searching for this timeout from: O(log N)
    auto it1 = std::lower_bound(m_timeouts.begin(), m_timeouts.end(), timeout, [](const TimeoutEvent* lhs, const TimeoutEvent* rhs) {
        return lhs->endTime < rhs->endTime;
    });

    // Search from the lower bound for the first timeout that matches the ID
    for (; it1 != m_timeouts.end(); ++it1) {
        if ((*it1)->id == id) {
            // We found it, erase this timeout.
            assert(timeout == (*it1)); // Sanity check
            m_timeouts.erase(it1);
            m_timeoutIds.erase(it0);
            id.invalidate();
            delete timeout;
            return true;
        }
        if ((*it1)->endTime > timeout->endTime) {
            // m_timeouts is sorted by endTime, therefor all subsequent timeouts are not going to match. Stop searching
            break;
        }
    }

    return false;
}

bool EventDispatcher::clearInterval(TimerId& id) {
    if (!id) {
        return true;
    }
    auto it0 = m_intervalIds.find(id);
    if (it0 == m_intervalIds.end()) {
        return false;
    }

    IntervalEvent* interval = it0->second;
    assert(interval->id == id);

    m_intervalIds.erase(it0);
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
