
#ifndef WORLDENGINE_ENTITY_H
#define WORLDENGINE_ENTITY_H

#include "core/core.h"

#include <entt/entt.hpp>

struct EntityNameComponent {
    std::string name;
};

class EntityEventDispatcher {
public:
    entt::dispatcher dispatcher;
};

class Scene;

class Entity {
    friend class Scene;
public:
    Entity(Scene* scene, const entt::entity& entity);

    Entity(const Entity& entity);

    Entity(Entity&& entity) noexcept;

    Entity(std::nullptr_t);

    Entity();

    ~Entity();

    template<typename T, typename... Args>
    T& addComponent(Args&&... args) const;

    template<typename T, typename... Args>
    T& setComponent(Args&&... args) const;

    template<typename T>
    bool removeComponent() const;

    template<typename T>
    T& getComponent() const;

    template<typename T>
    T* tryGetComponent() const;

    template<typename T, typename... Ts>
    std::tuple<T*, Ts*...> tryGetComponents() const;

    template<typename T>
    bool hasComponent() const;

    void destroy() const;

    const std::string& getName() const;

    Scene* getScene() const;

    bool exists() const;

    operator entt::entity() const;

    operator uint64_t() const;

    operator bool() const;

    void operator=(std::nullptr_t);

    Entity& operator=(const Entity& entity);

    bool operator==(const Entity& entity) const;

    bool operator!=(const Entity& entity) const;

    bool operator==(std::nullptr_t) const;

    bool operator!=(std::nullptr_t) const;

private:
    //void addRef();
    //
    //void removeRef();

    entt::registry& registry() const;

protected:
    entt::entity m_entity = entt::null;
    Scene* m_scene;
};



struct EntityDestroyEvent {
    Entity entity;
};



template<typename T, typename ...Args>
inline T& Entity::addComponent(Args && ...args) const {
#if _DEBUG
    if (hasComponent<T>()) {
		printf("Entity::addComponent() : Component type \"%s\" has already been added to this m_entity\n", typeid(T).name());
		assert(false);
	}
#endif
    T& component = registry().emplace<T>(m_entity, std::forward<Args>(args)...);
    return component;
}

template<typename T, typename ...Args>
inline T& Entity::setComponent(Args && ...args) const {
    T& component = registry().emplace_or_replace<T>(m_entity, std::forward<Args>(args)...);
    return component;
}

template<typename T>
inline bool Entity::removeComponent() const {
    size_t removed = registry().remove<T>(m_entity);
    return removed != 0;
}

template<typename T>
inline T& Entity::getComponent() const {
#if _DEBUG
    if (!hasComponent<T>()) {
		printf("Entity::getComponent() : Component type \"%s\" is not attached to this m_entity\n", typeid(T).name());
		assert(false);
	}
#endif
    return registry().get<T>(m_entity);
}

template<typename T>
inline T* Entity::tryGetComponent() const {
    return registry().try_get<T>(m_entity);
}

template<typename T, typename ...Ts>
inline std::tuple<T*, Ts*...> Entity::tryGetComponents() const {
    return registry().try_get<T, Ts...>(m_entity);
}

template<typename T>
inline bool Entity::hasComponent() const {
    return tryGetComponent<T>() != nullptr;
}

#endif //WORLDENGINE_ENTITY_H
