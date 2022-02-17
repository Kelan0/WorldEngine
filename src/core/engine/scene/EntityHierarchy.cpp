#include "EntityHierarchy.h"
#include "Scene.h"

bool EntityHierarchy::hasParent(const Entity& entity) {
    return getParent(entity) != nullptr;
}

Entity EntityHierarchy::getParent(const Entity& entity) {
    return getNode(entity).m_parent;
}

bool EntityHierarchy::hasChildren(const Entity& entity) {
    return getChildCount(entity) > 0;
}

uint32_t EntityHierarchy::getChildCount(const Entity& entity) {
    return getNode(entity).m_childCount;
}

Entity EntityHierarchy::getChild(const Entity& entity, uint32_t index) {
    return *(begin(entity) += index);
}

Entity EntityHierarchy::getFirstChild(const Entity& entity) {
    return getNode(entity).m_firstChild;
}

Entity EntityHierarchy::getLastChild(const Entity& entity) {
    return getNode(entity).m_lastChild;
}

Entity EntityHierarchy::getNextSibling(const Entity& entity) {
    return getNode(entity).m_nextSibling;
}

Entity EntityHierarchy::getPrevSibling(const Entity& entity) {
    return getNode(entity).m_prevSibling;
}

EntityHierarchy::ChildIterator EntityHierarchy::begin(const Entity& entity) {
    return ChildIterator(getNode(entity).m_firstChild);
}

EntityHierarchy::ChildIterator EntityHierarchy::end(const Entity& entity) {
    return ChildIterator(getNode(entity).m_lastChild);
}

bool EntityHierarchy::isParent(const Entity& entity, const Entity& parent) {
    if (parent == nullptr)
        return false;
    return parent == getParent(entity);
}

bool EntityHierarchy::isChild(const Entity& entity, const Entity& child) {
    if (child == nullptr)
        return false;
    return isParent(child, entity);
}

bool EntityHierarchy::isSibling(const Entity& entity, const Entity& sibling) {
    if (sibling == nullptr)
        return false;
    return getParent(entity) == getParent(sibling);
}

bool EntityHierarchy::isDescendant(const Entity& entity, const Entity& descendant) {
    if (descendant == nullptr)
        return false;

    Entity parentEntity = descendant;
    do {
        parentEntity = getParent(parentEntity);
        if (parentEntity == entity)
            return true;

    } while (parentEntity != nullptr);

    return false;
}

bool EntityHierarchy::isAncestor(const Entity& entity, const Entity& ancestor) {
    if (ancestor == nullptr)
        return false;
    return isDescendant(ancestor, entity);
}

bool EntityHierarchy::attachChild(const Entity& entity, const Entity& child) {
    if (child == nullptr || !child.hasComponent<EntityHierarchy>())
        return false;

    if (entity == child)
        return false; // Avoid circular tree - Entity cannot be a child of itself

    if (isAncestor(entity, child))
        return false; // Avoid circular tree - Ancestor cannot be a child of a descendant

    if (!detach(child)) {
        return false; // Failed to detach from existing parent.
    }

    EntityHierarchy& parentNode = getNode(entity);
    EntityHierarchy& childNode = getNode(child);

    if (parentNode.m_lastChild != nullptr)
        getNode(parentNode.m_lastChild).m_nextSibling = child;

    childNode.m_prevSibling = parentNode.m_lastChild;
    childNode.m_parent = entity;
    parentNode.m_lastChild = child;

    if (parentNode.m_childCount == 0)
        parentNode.m_firstChild = child;

    ++parentNode.m_childCount;

    return true;
}

bool EntityHierarchy::detachChild(const Entity& entity, const Entity& child) {
    if (child == nullptr)
        return false;

    if (!isChild(entity, child))
        return false;

    return detach(child);
}

bool EntityHierarchy::detach(const Entity& entity) {
    const Entity& child = entity;
    const Entity& parent = getParent(child);

    if (parent == nullptr)
        return true; // Already detached

    EntityHierarchy& childNode = getNode(child);
    EntityHierarchy& parentNode = getNode(parent);

    if (child == parentNode.m_firstChild)
        parentNode.m_firstChild = childNode.m_nextSibling;

    if (child == parentNode.m_lastChild)
        parentNode.m_lastChild = childNode.m_prevSibling;

    if (childNode.m_prevSibling != nullptr)
        getNode(childNode.m_prevSibling).m_nextSibling = childNode.m_nextSibling;

    if (childNode.m_nextSibling != nullptr)
        getNode(childNode.m_nextSibling).m_prevSibling = childNode.m_prevSibling;

    childNode.m_parent = nullptr;
    childNode.m_prevSibling = nullptr;
    childNode.m_nextSibling = nullptr;
    --parentNode.m_childCount;

    return true;
}

Entity EntityHierarchy::createChild(const Entity& entity, const std::string& name) {
    Entity child = entity.getScene()->createEntity(name);
    child.addComponent<EntityHierarchy>();
    attachChild(entity, child);
    return child;
}

EntityHierarchy& EntityHierarchy::getNode(const Entity& entity) {
#if _DEBUG
    if (entity == nullptr) {
        printf("Cannot get EntityHierarchy of NULL entity\n");
        assert(false);
    }
    if (!entity.hasComponent<EntityHierarchy>()) {
        printf("Cannot get EntityHierarchy of non-hierarchy entity\n");
        assert(false);
    }
#endif
    return entity.getComponent<EntityHierarchy>();
}







EntityHierarchy::ChildIterator::ChildIterator(const Entity& ptr):
    m_ptr(ptr) {
}

const Entity& EntityHierarchy::ChildIterator::operator*() const {
    return m_ptr;
}

Entity* EntityHierarchy::ChildIterator::operator->() {
    return &m_ptr;
}

EntityHierarchy::ChildIterator& EntityHierarchy::ChildIterator::operator++() {
    assert(m_ptr != nullptr);
    m_ptr = m_ptr.getComponent<EntityHierarchy>().m_nextSibling;
    return *this;
}

EntityHierarchy::ChildIterator EntityHierarchy::ChildIterator::operator++(int) {
    ChildIterator tmp = *this;
    ++(*this);
    return tmp;
}

EntityHierarchy::ChildIterator& EntityHierarchy::ChildIterator::operator+=(int i) {
    if (i > 0)
        for (; i > 0; --i) ++(*this);
    else
        for (; i < 0; ++i) --(*this);
    return *this;
}

EntityHierarchy::ChildIterator EntityHierarchy::ChildIterator::operator+(int i) {
    ChildIterator tmp = *this;
    tmp += i;
    return tmp;
}

EntityHierarchy::ChildIterator& EntityHierarchy::ChildIterator::operator--() {
    assert(m_ptr != nullptr);
    m_ptr = m_ptr.getComponent<EntityHierarchy>().m_prevSibling;
    return *this;
}

EntityHierarchy::ChildIterator EntityHierarchy::ChildIterator::operator--(int) {
    ChildIterator tmp = *this;
    --(*this);
    return tmp;
}

EntityHierarchy::ChildIterator& EntityHierarchy::ChildIterator::operator-=(int i) {
    return (*this) += (-i);
}

EntityHierarchy::ChildIterator EntityHierarchy::ChildIterator::operator-(int i) {
    return (*this) + (-i);
}

bool EntityHierarchy::ChildIterator::operator==(const ChildIterator& rhs) const {
    return m_ptr == rhs.m_ptr;
}

bool EntityHierarchy::ChildIterator::operator!=(const ChildIterator& rhs) const {
    return !(*this == rhs);
}
