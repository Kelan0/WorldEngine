
#ifndef WORLDENGINE_FRAMERESOURCE_H
#define WORLDENGINE_FRAMERESOURCE_H


#include "core/core.h"
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"

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

    T* operator*() const;

    T* operator->() const;

    T* operator[](const size_t& index) const;

    T* get(const size_t& index) const;

    T* get() const;

    void set(const size_t& index, T*&& resource);

    void set(T*&& resource);

    void set(ArrayType& resource);

    void reset();

    FrameResource<T>& operator=(ArrayType& resource);

    FrameResource<T>& operator=(T*&& resource);

    FrameResource<T>& operator=(std::nullptr_t);

    bool operator==(std::nullptr_t);

    bool operator!=(std::nullptr_t);

    operator bool();

    template<typename ...Args>
    static bool create(FrameResource<T>& outResource, Args&&... args);
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
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i)
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
inline T* FrameResource<T>::operator*() const {
    return get();
}

template<typename T>
inline T* FrameResource<T>::operator->() const {
    return get();
}

template<typename T>
inline T* FrameResource<T>::operator[](const size_t& index) const {
    return ArrayType::at(index);
}

template<typename T>
inline T* FrameResource<T>::get(const size_t& index) const {
    return ArrayType::at(index);
}

template<typename T>
inline T* FrameResource<T>::get() const {
    return ArrayType::at(Application::instance()->graphics()->getCurrentFrameIndex());
}

template<typename T>
inline void FrameResource<T>::set(const size_t& index, T*&& resource) {
    if (get(index) == resource)
        return;
    delete ArrayType::at(index);
    ArrayType::at(index) = std::exchange(resource, nullptr);
}

template<typename T>
inline void FrameResource<T>::set(T*&& resource) {
    set(Application::instance()->graphics()->getCurrentFrameIndex(), std::move(resource));
}

template<typename T>
inline void FrameResource<T>::set(ArrayType& resource) {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i)
        set(i, std::move(resource[i]));
}

template<typename T>
inline void FrameResource<T>::reset() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i)
        set(i, nullptr);
}

template<typename T>
inline FrameResource<T>& FrameResource<T>::operator=(ArrayType& resource) {
    //reset();
    set(resource);
    return *this;
}

template<typename T>
inline FrameResource<T>& FrameResource<T>::operator=(T*&& resource) {
    set(std::move(resource));
    return *this;
}

template<typename T>
inline FrameResource<T>& FrameResource<T>::operator=(std::nullptr_t) {
    reset();
    return *this;
}

template<typename T>
inline bool FrameResource<T>::operator==(std::nullptr_t) {
    return get() == nullptr;
}

template<typename T>
inline bool FrameResource<T>::operator!=(std::nullptr_t) {
    return !((*this) == nullptr);
}

template<typename T>
inline FrameResource<T>::operator bool() {
    return (*this) != nullptr;
}

template<typename T>
template<typename ...Args>
inline bool FrameResource<T>::create(FrameResource<T>& outResource, Args && ...args) {
    ArrayType resource;

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        resource[i] = T::create(std::forward<Args>(args)...);
        if (resource[i] == NULL) {
            for (; i >= 0; --i) delete resource[i];
            return false;
        }
    }

    outResource = resource;
    return true;
}
#endif //WORLDENGINE_FRAMERESOURCE_H
