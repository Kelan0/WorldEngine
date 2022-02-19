#pragma once

#include "../../../core.h"
#include <entt/signal/dispatcher.hpp>

class EventDispacher {
private:

	template<class Event>
	struct Listener {
		void(*callback)(const Event&);

		void receive(const Event& event);

		static size_t hash(void(*callback)(const Event&));
	};

	template<class Event, typename T>
	struct InstanceListener {
		T* instance;
		void(T::* callback)(const Event&);

		void receive(const Event& event);

		static size_t hash(void(T::* callback)(const Event&), T* instance);
	};

public:
	EventDispacher();

	~EventDispacher();

	template<class Event>
	void connect(void(*callback)(const Event&));

	template<typename Event, typename T>
	void connect(void(T::* callback)(const Event&), T* instance);

	template<class Event>
	void disconnect(void(*callback)(const Event&));

	template<class Event, typename T>
	void disconnect(void(T::* callback)(const Event&), T* instance);

	template<class Event>
	void trigger(const Event& event);

private:
	entt::dispatcher m_dispacher;
	std::unordered_map<std::type_index, std::unordered_map<size_t, void*>> m_eventListeners;
	std::unordered_map<std::type_index, std::unordered_map<size_t, void*>> m_memberEventListeners;
};

template<class Event>
inline void EventDispacher::connect(void(*callback)(const Event&)) {
	if (callback == NULL)
		return;

	auto& listeners = m_eventListeners[typeid(Event)];

	size_t key = Listener<Event>::hash(callback);
	if (listeners.find(key) != listeners.end())
		return; // This key was already inserted, so don't connect the callback a second time

	Listener<Event>* listener = new Listener<Event>();
	listener->callback = callback;
	m_dispacher.sink<Event>().connect<&Listener<Event>::receive>(*listener);
	listeners.insert(std::make_pair(key, static_cast<void*>(listener)));
}

template<typename Event, typename T>
inline void EventDispacher::connect(void(T::* callback)(const Event&), T* instance) {
	if (instance == NULL)
		return;

	if (callback == NULL)
		return;

	auto& listeners = m_memberEventListeners[typeid(Event)];

	size_t key = InstanceListener<Event, T>::hash(callback, instance);
	auto it = listeners.find(key);
	if (it != listeners.end())
		return; // This key was already inserted, so don't connect the callback a second time

	InstanceListener<Event, T>* listener = new InstanceListener<Event, T>();
	listener->callback = callback;
	listener->instance = instance;
	m_dispacher.sink<Event>().connect<&InstanceListener<Event, T>::receive>(*listener);
	listeners.insert(std::make_pair(key, static_cast<void*>(listener)));
}

template<class Event>
inline void EventDispacher::disconnect(void(*callback)(const Event&)) {
	auto it = m_eventListeners.find(std::type_index(typeid(Event)));
	if (it == m_eventListeners.end())
		return;

	auto& listeners = it->second;
	size_t key = Listener<Event>::hash(callback);
	auto it1 = listeners.find(key);
	if (it1 == listeners.end()) 
		return; // Already disconnected, or was never connected in the first place

	Listener<Event>* listener = static_cast<Listener<Event>*>(it1->second);
	listeners.erase(it1);

	if (listener == NULL)
		return; // Error?

	m_dispacher.sink<Event>().disconnect<&Listener<Event>::receive>(*listener);
	delete listener;
}

template<class Event, typename T>
inline void EventDispacher::disconnect(void(T::* callback)(const Event&), T* instance) {
	auto it = m_memberEventListeners.find(std::type_index(typeid(Event)));
	if (it == m_memberEventListeners.end())
		return;

	auto& listeners = it->second;
	size_t key = InstanceListener<Event, T>::hash(callback, instance);
	auto it1 = listeners.find(key);
	if (it1 == listeners.end())
		return; // Already disconnected, or was never connected in the first place

	InstanceListener<Event, T>* listener = static_cast<InstanceListener<Event, T>*>(it1->second);
	listeners.erase(it1);

	if (listener == NULL)
		return;

	m_dispacher.sink<Event>().disconnect<&InstanceListener<Event, T>::receive>(*listener);
	delete listener;
}

template<class Event>
inline void EventDispacher::trigger(const Event& event) {
	m_dispacher.trigger<Event>(event);
}


template<class Event>
inline void EventDispacher::Listener<Event>::receive(const Event& event) {
	reinterpret_cast<void(*)(const Event&)>(callback)(event);
}

template<class Event>
inline size_t EventDispacher::Listener<Event>::hash(void(*callback)(const Event&)) {
	size_t s = 0;
	std::hash_combine(s, static_cast<void*>(callback));
	return s;
}

template<class Event, typename T>
inline void EventDispacher::InstanceListener<Event, T>::receive(const Event& event) {
	(instance->*callback)(event);
}

template<class Event, typename T>
inline size_t EventDispacher::InstanceListener<Event, T>::hash(void(T::*callback)(const Event&), T* instance) {
	size_t s = 0;
	std::hash_combine(s, instance);
	std::hash_combine(s, std::type_index(typeid(callback)));
	return s;
}
