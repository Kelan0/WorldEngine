
#include "EntityChangeTracker.h"

void EntityChangeTracker::ensureCapacity(const entity_index& maxEntities) {
    m_changedFlags.ensureSize(maxEntities, true);
}

bool EntityChangeTracker::hasChanged(const entity_index& entityIndex) {
    if (entityIndex >= m_changedFlags.size())
        return true;

    return m_changedFlags.get(entityIndex);
}

void EntityChangeTracker::setChanged(const entity_index& entityIndex, const bool& changed) {
    if (entityIndex == INVALID_INDEX)
        return;

    m_changedFlags.expand(entityIndex, true);
    m_changedFlags.set(entityIndex, changed);
}

void EntityChangeTracker::setChanged(const EntityChangeTracker::entity_index& entityIndex, const size_t& count, const bool& changed) {
    PROFILE_SCOPE("EntityChangeTracker::setChanged");
    if (entityIndex == INVALID_INDEX)
        return;

    if (count == 0)
        return;

    if (SIZE_MAX - count < entityIndex)
        return;

    m_changedFlags.expand(entityIndex + count, true);
    m_changedFlags.set(entityIndex, count, changed);
}

void EntityChangeTracker::reindex(entity_index& entityIndex, const entity_index& newEntityIndex) {
    if (entityIndex != INVALID_INDEX) {
        // TODO: swap the values around
    }
    setChanged(entityIndex, true);
    entityIndex = newEntityIndex;
    setChanged(entityIndex, true);
}
