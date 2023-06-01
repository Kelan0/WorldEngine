
#ifndef WORLDENGINE_FRAMERESOURCE_H
#define WORLDENGINE_FRAMERESOURCE_H


#include "core/core.h"
#include "core/application/Engine.h"
#include <type_traits>

// FrameResource holds CONCURRENT_FRAMES number of T pointers (or T references if is_ptr is false), and automatically
// cycles through them on a frame-by-frame basis. Access via operator->(), operator*() or get() will access the resource
// for the current frame according to GraphicsManager::getCurrentFrameIndex(). This is so that Vulkan may have multiple
// frames in-flight simultaneously without them clashing over using the same resource. This class automates that.
template<typename T, bool is_ptr = true>
class FrameResource : protected std::array<std::conditional_t<is_ptr, T*, T>, CONCURRENT_FRAMES> {
    using Type = std::conditional_t<is_ptr, T*, T>;
    typedef std::array<Type, CONCURRENT_FRAMES> ArrayType;

private:
    FrameResource(const FrameResource& copy) = delete; // No copy

public:
    FrameResource(ArrayType& resource);

    FrameResource(FrameResource&& move) noexcept ;

    FrameResource(std::nullptr_t);

    FrameResource();

    ~FrameResource();

    void initDefault();

    const Type& operator*() const;

    const Type& operator->() const;

    const Type& operator[](size_t index) const;

    const Type& get(size_t index) const;

    const Type& get() const;

    void set(size_t index, const Type& resource);

    void set(const Type& resource);

    void set(const ArrayType& resource);

    void reset();

    FrameResource<T, is_ptr>& operator=(const ArrayType& resource);

    FrameResource<T, is_ptr>& operator=(const Type& resource);

    FrameResource<T, is_ptr>& operator=(std::nullptr_t);

    bool operator==(std::nullptr_t);

    bool operator!=(std::nullptr_t);

    operator bool();

    template<typename ...Args>
    static bool create(FrameResource<T, is_ptr>& outResource, Args&&... args);
};

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>::FrameResource(ArrayType& resource):
    FrameResource(nullptr) {
    set(resource);
}

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>::FrameResource(FrameResource&& move) noexcept {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        ArrayType::at(i) = std::exchange(move.at(i), Type{});
    }
//    set(move);
}

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>::FrameResource(std::nullptr_t) {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        if constexpr (is_ptr) {
            ArrayType::at(i) = nullptr;
        } else {
            ArrayType::at(i) = T{};
        }
    }
}

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>::FrameResource():
        FrameResource(nullptr) {
}

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>::~FrameResource() {
    reset();
}

template<typename T, bool is_ptr>
void FrameResource<T, is_ptr>::initDefault() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        set(i, new T());
    }
}

template<typename T, bool is_ptr>
inline const FrameResource<T, is_ptr>::Type& FrameResource<T, is_ptr>::operator*() const {
    return get();
}

template<typename T, bool is_ptr>
inline const FrameResource<T, is_ptr>::Type& FrameResource<T, is_ptr>::operator->() const {
    return get();
}

template<typename T, bool is_ptr>
inline const FrameResource<T, is_ptr>::Type& FrameResource<T, is_ptr>::operator[](size_t index) const {
    return ArrayType::at(index);
}

template<typename T, bool is_ptr>
inline const FrameResource<T, is_ptr>::Type& FrameResource<T, is_ptr>::get(size_t index) const {
    return ArrayType::at(index);
}

template<typename T, bool is_ptr>
inline const FrameResource<T, is_ptr>::Type& FrameResource<T, is_ptr>::get() const {
    return ArrayType::at(Engine::instance()->getSwapchainFrameIndex());
}

template<typename T, bool is_ptr>
inline void FrameResource<T, is_ptr>::set(size_t index, const Type& resource) {
    if (get(index) == resource)
        return;
    if constexpr (is_ptr)
        delete ArrayType::at(index);
//    ArrayType::at(index) = std::exchange(resource, nullptr);
    ArrayType::at(index) = resource;
}

template<typename T, bool is_ptr>
inline void FrameResource<T, is_ptr>::set(const Type& resource) {
    set(Engine::instance()->getSwapchainFrameIndex(), std::move(resource));
}

template<typename T, bool is_ptr>
inline void FrameResource<T, is_ptr>::set(const ArrayType& resource) {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i)
        set(i, std::move(resource[i]));
}

template<typename T, bool is_ptr>
inline void FrameResource<T, is_ptr>::reset() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        if constexpr (is_ptr) {
            set(i, nullptr);
        } else {
            set(i, T{});
        }
    }
}

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>& FrameResource<T, is_ptr>::operator=(const ArrayType& resource) {
    //reset();
    set(resource);
    return *this;
}

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>& FrameResource<T, is_ptr>::operator=(const Type& resource) {
    set(resource);
    return *this;
}

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>& FrameResource<T, is_ptr>::operator=(std::nullptr_t) {
    reset();
    return *this;
}

template<typename T, bool is_ptr>
inline bool FrameResource<T, is_ptr>::operator==(std::nullptr_t) {
    if constexpr (is_ptr) {
        return get() == nullptr;
    } else {
        return get() == T{};
    }
}

template<typename T, bool is_ptr>
inline bool FrameResource<T, is_ptr>::operator!=(std::nullptr_t) {
    return !((*this) == nullptr);
}

template<typename T, bool is_ptr>
inline FrameResource<T, is_ptr>::operator bool() {
    return (*this) != nullptr;
}

template<typename T, bool is_ptr>
template<typename ...Args>
inline bool FrameResource<T, is_ptr>::create(FrameResource<T, is_ptr>& outResource, Args && ...args) {
    ArrayType resource;

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        resource[i] = T::create(std::forward<Args>(args)...);

        if constexpr (is_ptr) {
            if (resource[i] == nullptr) {
                for (; i >= 0; --i) delete resource[i];
                return false;
            }
        }
    }

    outResource = resource;
    return true;
}

#endif //WORLDENGINE_FRAMERESOURCE_H
