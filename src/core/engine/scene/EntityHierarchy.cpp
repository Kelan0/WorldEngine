#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/Scene.h"

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

EntityHierarchy::iterator EntityHierarchy::begin(const Entity& entity) {
    return iterator(getNode(entity).m_firstChild, false);
}

EntityHierarchy::iterator EntityHierarchy::end(const Entity& entity) {
    return iterator(nullptr, false);
}

EntityHierarchy::iterator EntityHierarchy::rbegin(const Entity& entity) {
    return iterator(getNode(entity).m_lastChild, true);
}

EntityHierarchy::iterator EntityHierarchy::rend(const Entity& entity) {
    return iterator(nullptr, true);
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

    //printf("Detaching entity %llu from parent %llu\n", (uint64_t)entity, (uint64_t)parent);

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

Entity EntityHierarchy::create(Scene* scene, const std::string& name) {
    const Entity& entity = scene->createEntity(name);
    entity.addComponent<EntityHierarchy>();
    return entity;
}

Entity EntityHierarchy::createChild(const Entity& entity, const std::string& name) {
    const Entity& child = create(entity.getScene(), name);
    attachChild(entity, child);
    return child;
}

EntityHierarchy& EntityHierarchy::getNode(const Entity& entity) {
#if _DEBUG
    if (entity == nullptr) {
        printf("Cannot create EntityHierarchy of NULL entity\n");
        assert(false);
    }
    if (!entity.hasComponent<EntityHierarchy>()) {
        printf("Cannot create EntityHierarchy of non-hierarchy entity\n");
        assert(false);
    }
#endif
    return entity.getComponent<EntityHierarchy>();
}







EntityHierarchy::iterator::iterator(const Entity& ptr, bool reverse):
        m_ptr(ptr),
        m_reverse(reverse) {
    if (m_ptr != nullptr) {
        const EntityHierarchy& node = m_ptr.getComponent<EntityHierarchy>();
        m_prev = node.m_prevSibling;
        m_next = node.m_nextSibling;
    }
}

const Entity& EntityHierarchy::iterator::operator*() const {
    return m_ptr;
}

Entity* EntityHierarchy::iterator::operator->() {
    return &m_ptr;
}

EntityHierarchy::iterator& EntityHierarchy::iterator::operator++() {
    assert(m_ptr != nullptr);
    m_reverse ? prev() : next();
    return *this;
}

EntityHierarchy::iterator EntityHierarchy::iterator::operator++(int) {
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

EntityHierarchy::iterator& EntityHierarchy::iterator::operator+=(int i) {
    if (i > 0)
        for (; i > 0; --i) ++(*this);
    else
        for (; i < 0; ++i) --(*this);
    return *this;
}

EntityHierarchy::iterator EntityHierarchy::iterator::operator+(int i) {
    iterator tmp = *this;
    tmp += i;
    return tmp;
}

EntityHierarchy::iterator& EntityHierarchy::iterator::operator--() {
    assert(m_ptr != nullptr);
    m_reverse ? next() : prev();
    return *this;
}

EntityHierarchy::iterator EntityHierarchy::iterator::operator--(int) {
    iterator tmp = *this;
    --(*this);
    return tmp;
}

EntityHierarchy::iterator& EntityHierarchy::iterator::operator-=(int i) {
    return (*this) += (-i);
}

EntityHierarchy::iterator EntityHierarchy::iterator::operator-(int i) {
    return (*this) + (-i);
}

bool EntityHierarchy::iterator::operator==(const iterator& rhs) const {
    return m_ptr == rhs.m_ptr && m_reverse == rhs.m_reverse;
}

bool EntityHierarchy::iterator::operator!=(const iterator& rhs) const {
    return !(*this == rhs);
}

void EntityHierarchy::iterator::next() {
    m_prev = m_ptr;
    m_ptr = m_next;
    m_next = m_ptr == nullptr ? nullptr : m_ptr.getComponent<EntityHierarchy>().m_nextSibling;
}

void EntityHierarchy::iterator::prev() {
    m_next = m_ptr;
    m_ptr = m_prev;
    m_prev = m_ptr == nullptr ? nullptr : m_ptr.getComponent<EntityHierarchy>().m_prevSibling;
}