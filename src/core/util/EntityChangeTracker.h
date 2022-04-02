
#ifndef WORLDENGINE_ENTITYCHANGETRACKER_H
#define WORLDENGINE_ENTITYCHANGETRACKER_H

#include "core/core.h"
#include "core/util/DenseFlagArray.h"

class EntityChangeTracker {
public:
    typedef uint32_t entity_index;

    static constexpr entity_index INVALID_INDEX = (entity_index)(-1);

public:
    EntityChangeTracker() = default;

    ~EntityChangeTracker() = default;

    void ensureCapacity(const entity_index& maxEntities);

    bool hasChanged(const entity_index& entityIndex);

    void setChanged(const entity_index& entityIndex, const bool& changed);

    void setChanged(const entity_index& entityIndex, const size_t& count, const bool& changed);

    void reindex(entity_index& entityIndex, const entity_index& newEntityIndex);

private:
    DenseFlagArray m_changedFlags;
};


#endif //WORLDENGINE_ENTITYCHANGETRACKER_H
