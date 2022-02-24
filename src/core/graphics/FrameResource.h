#pragma once

#include "../core.h"
#include "../application/Application.h"
#include "GraphicsManager.h"

template<typename T>
class FrameResource : protected std::array<T*, CONCURRENT_FRAMES> {
	typedef std::array<T*, CONCURRENT_FRAMES> ArrayType;

private:
	FrameResource(const FrameResource& copy) = delete; // No copy

public:
	FrameResource(ArrayType& resource);

	FrameResource(FrameResource&& move);

	FrameResource(std::nullptr_t);

	FrameResource();

	~FrameResource();

	T* operator->() const;

	T* operator[](int index) const;

	T* get(int index) const;

	T* get() const;

	void reset();

	FrameResource<T>& operator=(ArrayType& resource);

	FrameResource<T>& operator=(std::nullptr_t);

	template<typename ...Args>
	static bool create(FrameResource<T>& outResource, Args&&... args);

private:
	void set(ArrayType& resource);
};

template<typename T>
inline FrameResource<T>::FrameResource(ArrayType& resource) {
	set(resource);
}

template<typename T>
inline FrameResource<T>::FrameResource(FrameResource&& move) {
	set(move);
}

template<typename T>
inline FrameResource<T>::FrameResource(std::nullptr_t) {
	for (int i = 0; i < CONCURRENT_FRAMES; ++i)
		ArrayType::at(i) = nullptr;
}

template<typename T>
inline FrameResource<T>::FrameResource():
	FrameResource(nullptr) {
}

template<typename T>
inline FrameResource<T>::~FrameResource() {
	reset();
}

template<typename T>
inline T* FrameResource<T>::operator->() const {
	return get();
}

template<typename T>
inline T* FrameResource<T>::operator[](int index) const {
	return ArrayType::at(index);
}

template<typename T>
inline T* FrameResource<T>::get(int index) const {
	return ArrayType::at(index);
}

template<typename T>
inline T* FrameResource<T>::get() const {
	return ArrayType::at(Application::instance()->graphics()->getCurrentFrameIndex());
}

template<typename T>
inline void FrameResource<T>::reset() {
	for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
		delete ArrayType::at(i);
		ArrayType::at(i) = nullptr;
	}
}

template<typename T>
inline FrameResource<T>& FrameResource<T>::operator=(ArrayType& resource) {
	reset();
	set(resource);
	return *this;
}

template<typename T>
inline FrameResource<T>& FrameResource<T>::operator=(std::nullptr_t) {
	reset();
	return *this;
}

template<typename T>
inline void FrameResource<T>::set(ArrayType& resource) {
	for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
		ArrayType::at(i) = std::exchange(resource[i], nullptr);
	}
}

template<typename T>
template<typename ...Args>
inline bool FrameResource<T>::create(FrameResource<T>& outResource, Args && ...args) {
	ArrayType resource;

	for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
		resource[i] = T::create(std::forward<Args>(args)...);
		if (resource[i] == NULL) {
			for (; i >= 0; --i) delete resource[i];
			return false;
		}
	}

	outResource = resource;
	return true;
}
