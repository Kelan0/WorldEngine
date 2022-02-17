#pragma once

#include "../../core.h"
#include <entt/entt.hpp>

class Entity;

class Scene {
	friend class Entity;
public:

public:
	Scene();

	~Scene();

	void init();

	Entity createEntity(const std::string& name);

	void destroyEntity(const Entity& entity);

private:
	entt::registry m_registry;
	entt::dispatcher m_dispacher;
	std::unordered_map<entt::entity, std::vector<Entity*>> m_entityRefTracker;
};
