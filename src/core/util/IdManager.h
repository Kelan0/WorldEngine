
#ifndef WORLDENGINE_IDMANAGER_H
#define WORLDENGINE_IDMANAGER_H

#include <deque>
#include <type_traits>
#include <mutex>

template<typename ID, bool ThreadSafe = false>
class IdManager {
    static_assert(std::is_integral<ID>::value, "ID type must be an integer");
    static_assert(std::numeric_limits<ID>::max() > 1, "ID type is not large enough");

public:
    static const ID NULL_ID;

private:
    struct Interval {
        ID lower;
        ID upper;
    };
public:
    IdManager();

    ~IdManager();

    ID getID();

    bool freeID(ID id);

    bool isThreadSafe() const;

private:
    std::deque<Interval> m_freeIntervals;
    using mutex_t = std::conditional<ThreadSafe, std::mutex, uint8_t>::type;
    using lock_t = std::conditional<ThreadSafe, std::lock_guard<std::mutex>, uint8_t>::type;
    mutex_t m_mutex;
};



// Implementation taken from:
// https://stackoverflow.com/questions/2620218/fastest-container-or-algorithm-for-unique-reusable-ids-in-c

template<typename ID, bool ThreadSafe>
const ID IdManager<ID, ThreadSafe>::NULL_ID = (ID)0; // First ID is 1

template<typename ID, bool ThreadSafe>
IdManager<ID, ThreadSafe>::IdManager() {
    m_freeIntervals.emplace_back(Interval{1, std::numeric_limits<ID>::max()});
}

template<typename ID, bool ThreadSafe>
IdManager<ID, ThreadSafe>::~IdManager() = default;

template<typename ID, bool ThreadSafe>

ID IdManager<ID, ThreadSafe>::getID() {
    if (m_freeIntervals.empty())
        return NULL_ID;

    lock_t lock(m_mutex);

    Interval& first = m_freeIntervals.front();
    ID id = first.lower;

    ++first.lower;
    if (first.lower > first.upper)
        m_freeIntervals.pop_front();

    return id;
}

template<typename ID, bool ThreadSafe>
bool IdManager<ID, ThreadSafe>::freeID(ID id) {
    if (id == NULL_ID)
        return false;

    lock_t lock(m_mutex);

    // Find first interval where id >= lower
    auto it1 = std::upper_bound(m_freeIntervals.begin(), m_freeIntervals.end(), id, [](ID id, const Interval& interval) {
        return id < interval.lower;
    });

    if (it1 == m_freeIntervals.end())
        return false; // this shouldn't happen.

    bool merged = false;

    Interval& nextInterval = *it1;

    if (id + 1 == nextInterval.lower) {// Merge with next interval
        --nextInterval.lower;
        merged = true;
    }

    if (it1 != m_freeIntervals.begin()) {
        auto it0 = it1 - 1;
        Interval& prevInterval = *it0;

        if (id >= prevInterval.lower && id <= prevInterval.upper)
            return false; // ID was already within this free interval.

        if (id - 1 == prevInterval.upper) { // Merge with previous interval
            ++prevInterval.upper;
            merged = true;
        }

        if (prevInterval.upper == nextInterval.lower) { // Merge intervals
            nextInterval.lower = prevInterval.lower;
            m_freeIntervals.erase(it0);
        }
    }

    if (!merged) {
        m_freeIntervals.insert(it1, Interval{id, id});
    }

    return true;
}

template<typename ID, bool ThreadSafe>
bool IdManager<ID, ThreadSafe>::isThreadSafe() const {
    return ThreadSafe;
}


#endif //WORLDENGINE_IDMANAGER_H
