#pragma once

#include "Entity.h"
#include "Scene.h"

class EntityHierarchy {
public:
	class ChildIterator {
		friend class EntityHierarchy;
	private:
		ChildIterator(const Entity& ptr);

	public:
		const Entity& operator*() const;

		Entity* operator->();

		ChildIterator& operator++();

		ChildIterator operator++(int);

		ChildIterator& operator+=(int i);

		ChildIterator operator+(int i);

		ChildIterator& operator--();

		ChildIterator operator--(int);

		ChildIterator& operator-=(int i);

		ChildIterator operator-(int i);

		bool operator==(const ChildIterator& rhs) const;

		bool operator!=(const ChildIterator& rhs) const;

	private:
		Entity m_ptr;
	};
public:
	static bool hasParent(const Entity& entity);

	static Entity getParent(const Entity& entity);

	static bool hasChildren(const Entity& entity);

	static uint32_t getChildCount(const Entity& entity);

	static Entity getChild(const Entity& entity, uint32_t index);

	static Entity getFirstChild(const Entity& entity);

	static Entity getLastChild(const Entity& entity);

	static Entity getNextSibling(const Entity& entity);

	static Entity getPrevSibling(const Entity& entity);

	static ChildIterator begin(const Entity& entity);

	static ChildIterator end(const Entity& entity);

	static bool isParent(const Entity& entity, const Entity& parent);

	static bool isChild(const Entity& entity, const Entity& child);

	static bool isSibling(const Entity& entity, const Entity& sibling);

	static bool isDescendant(const Entity& entity, const Entity& descendant);

	static bool isAncestor(const Entity& entity, const Entity& ancestor);

	static bool attachChild(const Entity& entity, const Entity& child);

	static bool detachChild(const Entity& entity, const Entity& child);

	static bool detach(const Entity& entity);

	static Entity createChild(const Entity& entity, const std::string& name);

private:
	static EntityHierarchy& getNode(const Entity& entity);

private:
	Entity m_parent = nullptr;
	Entity m_firstChild = nullptr;
	Entity m_lastChild = nullptr;
	Entity m_prevSibling = nullptr;
	Entity m_nextSibling = nullptr;
	uint32_t m_childCount = 0;
};
