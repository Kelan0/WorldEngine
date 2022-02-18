#pragma once

#include "Entity.h"
#include "Scene.h"

class EntityHierarchy {
public:
	class iterator {
		friend class EntityHierarchy;
	private:
		iterator(const Entity& ptr, bool reverse);

	public:
		const Entity& operator*() const;

		Entity* operator->();

		iterator& operator++();

		iterator operator++(int);

		iterator& operator+=(int i);

		iterator operator+(int i);

		iterator& operator--();

		iterator operator--(int);

		iterator& operator-=(int i);

		iterator operator-(int i);

		bool operator==(const iterator& rhs) const;

		bool operator!=(const iterator& rhs) const;

	private:
		void next();

		void prev();
	private:
		Entity m_ptr;
		Entity m_prev;
		Entity m_next;
		bool m_reverse;
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

	static iterator begin(const Entity& entity);

	static iterator end(const Entity& entity);

	static iterator rbegin(const Entity& entity);

	static iterator rend(const Entity& entity);

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
