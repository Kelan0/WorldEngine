#include "Entity.h"
#include "Scene.h"

Entity::Entity(Scene* scene, const entt::entity& entity) :
	m_scene(scene),
	m_entity(entity) {
	addRef();
}

Entity::Entity(const Entity& entity): 
	Entity(entity.m_scene, entity.m_entity) {
}

Entity::Entity(Entity&& entity) noexcept {
	entity.removeRef();
	m_scene = std::exchange(entity.m_scene, nullptr);
	m_entity = std::exchange(entity.m_entity, entt::null);
	addRef();
}

Entity::Entity(std::nullptr_t):
	Entity(NULL, entt::null) {
}

Entity::Entity(): 
	Entity(NULL, entt::null) {
}

Entity::~Entity() {
	removeRef();
}

void Entity::destroy() const {
	m_scene->destroyEntity(*this);
}

const std::string& Entity::getName() const {
	return getComponent<EntityNameComponent>().name;
}

Scene* Entity::getScene() const {
	return m_scene;
}

bool Entity::exists() const {
	return (m_entity != entt::null) && (m_scene != NULL);
}

Entity::operator entt::entity() const {
	return m_entity;
}

Entity::operator uint64_t() const {
	return (uint64_t)m_entity;
}

Entity::operator bool() const {
	return this->exists();
}

void Entity::operator=(std::nullptr_t) {
	removeRef();
	m_entity = entt::null;
	m_scene = NULL;
}

Entity& Entity::operator=(const Entity& entity) {
	removeRef();
	m_entity = entity.m_entity;
	m_scene = entity.m_scene;
	addRef();
	return *this;
}

bool Entity::operator==(const Entity& entity) const {
	return m_scene == entity.m_scene && m_entity == entity.m_entity;
}

bool Entity::operator!=(const Entity& entity) const {
	return !(*this == entity);
}

bool Entity::operator==(std::nullptr_t) const {
	return m_entity == entt::null;
}

bool Entity::operator!=(std::nullptr_t) const {
	return !(*this == nullptr);
}

void Entity::addRef() {
	if (m_entity != entt::null) {
		//printf("addRef 0x%p for %llu\n", this, (uint64_t)m_entity);
		m_scene->m_entityRefTracker[m_entity].emplace_back(this);
	}
}

void Entity::removeRef() {
	if (m_entity != entt::null) {
		//printf("removeRef 0x%p for %llu\n", this, (uint64_t)m_entity);
		auto& v = m_scene->m_entityRefTracker[m_entity];
		v.erase(std::remove(v.begin(), v.end(), this), v.end());
	}
}

entt::registry& Entity::registry() const {
	return m_scene->m_registry;
}
