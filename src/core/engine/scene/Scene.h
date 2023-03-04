
#ifndef WORLDENGINE_SCENE_H
#define WORLDENGINE_SCENE_H

#include "core/core.h"
#include "core/engine/scene/Entity.h"
#include "core/engine/event/EventDispatcher.h"
#include <entt/entt.hpp>

struct ScreenResizeEvent;

template<class T>
struct ComponentAddedEvent {
    Entity entity;
    T* component;
};

template<class T>
struct ComponentRemovedEvent {
    Entity entity;
    T* component;
};

class Scene {
    friend class Entity;
    friend class SceneRenderer;
public:
    typedef entt::registry Registry;

private:
    template<class Component>
    struct ComponentEvents {
        static void onConstruct(Scene* scene, entt::registry& registry, entt::entity entity);

        static void onDestroy(Scene* scene, entt::registry& registry, entt::entity entity);
    };

public:
    Scene();

    ~Scene();

    void init();

    void preTick(const double& dt);

    void destroyEntity(const Entity& entity);

    Entity createEntity();

    Entity createNamedEntity(const std::string& name);

    Entity findNamedEntity(const std::string& name);

    template<class Component>
    void enableEvents();

    template<class Component>
    void disableEvents();

    EventDispatcher* getEventDispatcher() const;

    entt::registry* registry();

    bool setMainCameraEntity(const Entity& entity);

    const Entity& getMainCameraEntity() const;

private:
    void onScreenResize(ScreenResizeEvent* event);

private:
    entt::registry m_registry;
    EventDispatcher* m_eventDispatcher;
    //std::unordered_map<entt::entity, std::vector<Entity*>> m_entityRefTracker;
    std::unordered_map<std::string, entt::entity> m_entityNameMap;

    Entity m_mainCameraEntity;
    Entity m_defaultCamera;
};

template<class Component>
inline void Scene::enableEvents() {
    m_registry.on_construct<Component>().template connect<&ComponentEvents<Component>::onConstruct>(this);
    m_registry.on_destroy<Component>().template connect<&ComponentEvents<Component>::onDestroy>(this);
}

template<class Component>
inline void Scene::disableEvents() {
    m_registry.on_construct<Component>().template disconnect<&ComponentEvents<Component>::onConstruct>(this);
    m_registry.on_destroy<Component>().template disconnect<&ComponentEvents<Component>::onDestroy>(this);
}

template<class Component>
inline void Scene::ComponentEvents<Component>::onConstruct(Scene* scene, entt::registry& registry, entt::entity entity) {
    ComponentAddedEvent<Component> event{};
    event.entity = Entity(scene, entity);
    event.component = &event.entity.template getComponent<Component>();
    scene->getEventDispatcher()->trigger(&event);
}

template<class Component>
inline void Scene::ComponentEvents<Component>::onDestroy(Scene* scene, entt::registry& registry, entt::entity entity) {
    ComponentRemovedEvent<Component> event;
    event.entity = Entity(scene, entity);
    event.component = &event.entity.template getComponent<Component>();
    scene->getEventDispatcher()->trigger(&event);
}

#endif //WORLDENGINE_SCENE_H
