#include "Scene.h"

#include "Entity.h"
#include "EntityHierarchy.h"

Scene::Scene() {
	m_eventDispacher = new EventDispacher();
}

Scene::~Scene() {
	for (auto it = m_entityRefTracker.begin(); it != m_entityRefTracker.end(); ++it) {
		auto& references = it->second;
		for (auto ref : references) {
			ref->m_entity = entt::null;
			ref->m_scene = NULL;
		}
	}
	m_entityRefTracker.clear();
}

void Scene::init() {
	enableEvents<EntityHierarchy>();

	//m_eventDispacher->connect<ComponentAddedEvent<EntityHierarchy>>([](const ComponentAddedEvent<EntityHierarchy>& event) {
	//	printf("EntityHierarchy added to entity %llu\n", (uint64_t)event.entity);
	//});

	m_eventDispacher->connect<ComponentRemovedEvent<EntityHierarchy>>([](const ComponentRemovedEvent<EntityHierarchy>& event) {
		printf("EntityHierarchy for entity %llu was deleted\n", (uint64_t)event.entity);
		for (auto it = EntityHierarchy::begin(event.entity); it != EntityHierarchy::end(event.entity); ++it)
			EntityHierarchy::detach(*it);
		EntityHierarchy::detach(event.entity);
	});
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

		auto& references = m_entityRefTracker[id];
		for (auto ref : references)
			ref->m_entity = entt::null;
		m_entityRefTracker.erase(id);

	}
}

EventDispacher* Scene::getEventDispacher() const {
	return m_eventDispacher;
}
