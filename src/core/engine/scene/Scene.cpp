#include "Scene.h"

#include "Entity.h"
#include "EntityHierarchy.h"

//void onEntityHierarchyNodeDestroy(Scene* scene, entt::registry& reg, entt::entity id) {
//	Entity entity(scene, id);
//	entity.getComponent<EntityHierarchyNode>().detach();
//}
// 
//void onEntityHierarchyNodeDestroy(Scene* scene, entt::registry& reg, entt::entity id) {
//	Entity entity(scene, id);
//	entity.getComponent<EntityHierarchyNode>().detach();
//}


Scene::Scene() {
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
	m_registry.on_destroy<EntityHierarchy>();
	m_registry.on_construct<EntityHierarchy>();
	//m_registry.on_destroy<EntityHierarchy>().connect<&onEntityHierarchyNodeDestroy>(this);
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
