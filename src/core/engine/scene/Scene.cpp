#include "Scene.h"

#include "Entity.h"
#include "EntityHierarchy.h"
#include "Camera.h"
#include "Transform.h"
#include "event/Events.h"
#include "../../application/Application.h"

Scene::Scene() {
	m_eventDispacher = new EventDispacher();
}

Scene::~Scene() {
	m_registry.clear();
	//for (auto it = m_entityRefTracker.begin(); it != m_entityRefTracker.end(); ++it) {
	//	auto& references = it->second;
	//	for (auto ref : references) {
	//		ref->m_entity = entt::null;
	//		ref->m_scene = NULL;
	//	}
	//}
	//m_entityRefTracker.clear();
}

void Scene::init() {
	enableEvents<EntityHierarchy>();

	m_eventDispacher->connect<ComponentRemovedEvent<EntityHierarchy>>([](const ComponentRemovedEvent<EntityHierarchy>& event) {
		for (auto it = EntityHierarchy::begin(event.entity); it != EntityHierarchy::end(event.entity); ++it)
			EntityHierarchy::detach(*it);
		EntityHierarchy::detach(event.entity);
	});

	m_eventDispacher->connect<ScreenResizeEvent>(&Scene::onScreenResize, this);

	m_defaultCamera = createEntity("Default Camera");
	m_defaultCamera.addComponent<Camera>().setFovDegrees(90.0).setClippingPlanes(0.05, 500.0);
	m_defaultCamera.addComponent<Transform>();
	setMainCamera(nullptr);
}

Entity Scene::createEntity(const std::string& name) {
	entt::entity id = m_registry.create();

	Entity entity(this, id);

	entity.addComponent<EntityNameComponent>(name);
	EntityEventDispacher& eventDispacher = entity.addComponent<EntityEventDispacher>();

	return entity;
}

void Scene::destroyEntity(const Entity& entity) {
	if (entity) {
		entity.getComponent<EntityEventDispacher>().dispacher.trigger<EntityDestroyEvent>(entity);

		entt::entity id = entity.m_entity;
		m_registry.destroy(id);

		//auto& references = m_entityRefTracker[id];
		//for (auto ref : references)
		//	ref->m_entity = entt::null;
		//m_entityRefTracker.erase(id);

	}
}

EventDispacher* Scene::getEventDispacher() const {
	return m_eventDispacher;
}

entt::registry* Scene::registry() {
	return &m_registry;
}

bool Scene::setMainCamera(const Entity& entity) {
	if (entity == nullptr) {
		m_mainCameraEntity = m_defaultCamera;
		return true;
	}

	if (!entity.hasComponent<Camera>() || !entity.hasComponent<Transform>()) {
		printf("Scene::setMainCamera - Entity \"%s\" must have a Camera and Transform component in order to be used as the main scene camera\n", entity.getName().c_str());
		return false;
	}
	m_mainCameraEntity = entity;
	return true;
}

const Entity& Scene::getMainCamera() const {
	return m_mainCameraEntity;
}

void Scene::onScreenResize(const ScreenResizeEvent& event) {
	printf("SCENE - screen resized\n");
	double aspectRatio = (double)event.newSize.x / (double)event.newSize.y;
	m_defaultCamera.getComponent<Camera>().setAspect(aspectRatio);
}
