#pragma once

#include "../../../core.h"
#include <entt/signal/dispatcher.hpp>

class EventDispacher {
private:
	template<class Event>
	struct Listener {
		void(*callback)(const Event&);
		void receive(const Event& event);
	};
public:
	EventDispacher();

	~EventDispacher();

	template<class Event>
	void connect(void(*callback)(const Event&));

	template<class Event>
	void disconnect(void(*callback)(const Event&));

	template<class Event>
	void trigger(const Event& event);

private:
	entt::dispatcher m_dispacher;
	std::unordered_map<std::type_index, std::unordered_map<void*, void*>> m_eventListeners;
};

template<class Event>
inline void EventDispacher::connect(void(*callback)(const Event&)) {
	if (callback == NULL)
		return;

	auto& listeners = m_eventListeners[typeid(Event)];

	if (listeners.find(static_cast<void*>(callback)) != listeners.end())
		return; // This callback has already been connected

	Listener<Event>* listener = new Listener<Event>();
	listener->callback = callback;
	m_dispacher.sink<Event>().connect<&Listener<Event>::receive>(*listener);

	listeners.insert(std::make_pair(static_cast<void*>(callback), listener));
}

template<class Event>
inline void EventDispacher::disconnect(void(*callback)(const Event&)) {
	auto it = m_eventListeners.find(std::type_index(typeid(Event)));
	if (it == m_eventListeners.end())
		return;

	auto& listeners = it->second;
	auto it1 = listeners.find(static_cast<void*>(callback));
	if (it1 == listeners.end())
		return;

	Listener<Event>* listener = static_cast<Listener<Event>*>(it1->second);
	if (listener == NULL)
		return;

	m_dispacher.sink<Event>().disconnect<Listener<Event>::receive>(*listener);
	delete listener;
	listeners.erase(it1);
}

template<class Event>
inline void EventDispacher::trigger(const Event& event) {
	m_dispacher.trigger<Event>(event);
}


template<class Event>
inline void EventDispacher::Listener<Event>::receive(const Event& event) {
	callback(event);
}
