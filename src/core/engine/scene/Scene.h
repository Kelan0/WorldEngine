#pragma once

#include "../../core.h"
#include "Entity.h"
#include "event/EventDispacher.h"
#include "event/Events.h"
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
	friend class SceneRenderer;
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

	Entity createEntity(const std::string& name);

	void destroyEntity(const Entity& entity);

	template<class Component>
	void enableEvents();

	template<class Component>
	void disableEvents();

	EventDispacher* getEventDispacher() const;

	entt::registry* registry();

	bool setMainCamera(const Entity& entity);

	const Entity& getMainCamera() const;

private:
	void onScreenResize(const ScreenResizeEvent& event);

private:
	entt::registry m_registry;
	EventDispacher* m_eventDispacher;
	//std::unordered_map<entt::entity, std::vector<Entity*>> m_entityRefTracker;

	Entity m_mainCameraEntity;
	Entity m_defaultCamera;
};

template<class Component>
inline void Scene::enableEvents() {
	m_registry.on_construct<Component>().connect<&ComponentEvents<Component>::onConstruct>(this);
	m_registry.on_destroy<Component>().connect<&ComponentEvents<Component>::onDestroy>(this);
}

template<class Component>
inline void Scene::disableEvents() {
	m_registry.on_construct<Component>().disconnect<&ComponentEvents<Component>::onConstruct>(this);
	m_registry.on_destroy<Component>().disconnect<&ComponentEvents<Component>::onDestroy>(this);
}

template<class Component>
inline void Scene::ComponentEvents<Component>::onConstruct(Scene* scene, entt::registry& registry, entt::entity entity) {
	ComponentAddedEvent<Component> event;
	event.entity = Entity(scene, entity);
	event.component = &event.entity.getComponent<Component>();
	scene->getEventDispacher()->trigger(event);
}

template<class Component>
inline void Scene::ComponentEvents<Component>::onDestroy(Scene* scene, entt::registry& registry, entt::entity entity) {
	ComponentRemovedEvent<Component> event;
	event.entity = Entity(scene, entity);
	event.component = &event.entity.getComponent<Component>();
	scene->getEventDispacher()->trigger(event);
}
