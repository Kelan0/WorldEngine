#include "core/engine/scene/Scene.h"
#include "core/engine/scene/Entity.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/event/ApplicationEvents.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"

Scene::Scene() {
    m_eventDispatcher = new EventDispatcher();
}

Scene::~Scene() {
    m_registry.clear();
}

bool Scene::init() {
    PROFILE_SCOPE("Scene::init")
    LOG_INFO("Initializing Scene");
    enableEvents<EntityHierarchy>();

    m_eventDispatcher->connect<ComponentRemovedEvent<EntityHierarchy>>([](ComponentRemovedEvent<EntityHierarchy>* event) {
        for (auto it = EntityHierarchy::begin(event->entity); it != EntityHierarchy::end(event->entity); ++it)
            EntityHierarchy::detach(*it);
        EntityHierarchy::detach(event->entity);
    });

    m_eventDispatcher->connect<ScreenResizeEvent>(&Scene::onScreenResize, this);

    m_defaultCamera = createNamedEntity("Default Camera");
    m_defaultCamera.addComponent<Camera>().setFovDegrees(90.0).setClippingPlanes(0.05, 500.0);
    m_defaultCamera.addComponent<Transform>();
    setMainCameraEntity(nullptr);

    return true;
}

void Scene::preTick(double dt) {
//    const auto& renderEntities = registry()->group<RenderComponent, RenderInfo, Transform>();
}

void Scene::destroyEntity(const Entity& entity) {
    PROFILE_SCOPE("Scene::destroyEntity")
    if (entity) {
        entity.getComponent<EntityEventDispatcher>().dispatcher.trigger<EntityDestroyEvent>(entity);
        EntityNameComponent* nameComponent = entity.tryGetComponent<EntityNameComponent>();
        if (nameComponent != nullptr) {
            m_entityNameMap.erase(nameComponent->name);
        }

        entt::entity id = entity.m_entity;
        m_registry.destroy(id);

        //auto& references = m_entityRefTracker[id];
        //for (auto ref : references)
        //	ref->m_entity = entt::null;
        //m_entityRefTracker.erase(id);

    }
}

Entity Scene::createEntity() {
    PROFILE_SCOPE("Scene::createEntity")
    entt::entity id = m_registry.create();
    Entity entity(this, id);
    EntityEventDispatcher& eventDispatcher = entity.addComponent<EntityEventDispatcher>();
    return entity;
}

Entity Scene::createNamedEntity(const std::string& name) {
    PROFILE_SCOPE("Scene::createNamedEntity")
    auto result = m_entityNameMap.insert(std::make_pair(name, entt::null));
    auto it = result.first;

    if (!result.second) {
        // Not inserted (already existed)
        LOG_WARN("Unable to create named entity \"%s\" for this scene because the name is already taken", name.c_str());
        return Entity{nullptr};
    }

    Entity entity = createEntity();
    entity.addComponent<EntityNameComponent>(name);

    it->second = (entt::entity)entity;

    return entity;
}

Entity Scene::findNamedEntity(const std::string& name) {
    auto it = m_entityNameMap.find(name);
    if (it == m_entityNameMap.end()) {
        return nullptr;
    }
    return Entity(this, it->second);
}

EventDispatcher* Scene::getEventDispatcher() const {
    return m_eventDispatcher;
}

entt::registry* Scene::registry() {
    return &m_registry;
}

bool Scene::setMainCameraEntity(const Entity& entity) {
    PROFILE_SCOPE("Scene::setMainCameraEntity")
    if (entity == nullptr) {
        m_mainCameraEntity = m_defaultCamera;
        return true;
    }

    if (!entity.hasComponent<Camera>() || !entity.hasComponent<Transform>()) {
        LOG_ERROR("Scene::setMainCameraEntity - Entity \"%s\" must have a Camera and Transform component in order to be used as the main scene camera", entity.getName().c_str());
        return false;
    }
    m_mainCameraEntity = entity;
    return true;
}

const Entity& Scene::getMainCameraEntity() const {
    return m_mainCameraEntity;
}

void Scene::onScreenResize(ScreenResizeEvent* event) {
    double aspectRatio = (double)event->newSize.x / (double)event->newSize.y;
    m_defaultCamera.getComponent<Camera>().setAspect(aspectRatio);
}
