#pragma once

#include "../../core.h"
#include "Entity.h"
#include "event/EventDispacher.h"
#include <entt/entt.hpp>

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
private:
	template<class Component>
	struct ComponentEventTracker {
		static void onConstruct(Scene* scene, entt::registry& registry, entt::entity entity);

		static void onDestroy(Scene* scene, entt::registry& registry, entt::entity entity);
	};

public:
	Scene();

	~Scene();

	void init();

	Entity createEntity(const std::string& name);

	void destroyEntity(const Entity& entity);

	template<class Component>
	void enableEvents();

	template<class Component>
	void disableEvents();

	EventDispacher* getEventDispacher() const;

private:
	entt::registry m_registry;
	entt::dispatcher m_dispacher;
	std::unordered_map<entt::entity, std::vector<Entity*>> m_entityRefTracker;
	EventDispacher* m_eventDispacher;
};

template<class Component>
inline void Scene::enableEvents() {
	m_registry.on_construct<Component>().connect<&ComponentEventTracker<Component>::onConstruct>(this);
	m_registry.on_destroy<Component>().connect<&ComponentEventTracker<Component>::onDestroy>(this);
}

template<class Component>
inline void Scene::disableEvents() {
	m_registry.on_construct<Component>().disconnect<&ComponentEventTracker<Component>::onConstruct>(this);
	m_registry.on_destroy<Component>().disconnect<&ComponentEventTracker<Component>::onDestroy>(this);
}

template<class Component>
inline void Scene::ComponentEventTracker<Component>::onConstruct(Scene* scene, entt::registry& registry, entt::entity entity) {
	ComponentAddedEvent<Component> event;
	event.entity = Entity(scene, entity);
	event.component = &event.entity.getComponent<Component>();
	scene->getEventDispacher()->trigger(event);
}

template<class Component>
inline void Scene::ComponentEventTracker<Component>::onDestroy(Scene* scene, entt::registry& registry, entt::entity entity) {
	ComponentRemovedEvent<Component> event;
	event.entity = Entity(scene, entity);
	event.component = &event.entity.getComponent<Component>();
	scene->getEventDispacher()->trigger(event);
}
