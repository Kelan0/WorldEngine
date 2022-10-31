#ifndef WORLDENGINE_GRAPHICSRESOURCE_H
#define WORLDENGINE_GRAPHICSRESOURCE_H

//#include <xmemory>
//#include <unordered_map>
//#include <string>
//#include <memory>
//#include <cassert>
#include "core/core.h"


typedef uint64_t ResourceId;


template<typename T>
class SharedResource;

template<typename T>
class WeakResource;

class GraphicsResource;

template<typename T>
class SharedResource {
    friend class WeakResource<T>;

    using ptr_t = T*;
    using ref_t = T&;
private:
    struct Tracker {
//        NO_COPY(Tracker);

        ptr_t ptr;
        std::_Atomic_counter_t strongRefCount = 1;
        std::_Atomic_counter_t weakRefCount = 1;
        std::unordered_map<SharedResource<T>*, std::string> m_ownerNames;

        Tracker(const ptr_t& ptr);

        ~Tracker();

        bool incrNotZero();

        void incrStrong();

        void decrStrong();

        void incrWeak();

        void decrWeak();

        void getAllReferenceOwnerNames(std::vector<std::string>& outOwnerNames) const;
    };

public:
    SharedResource();

    SharedResource(std::nullptr_t);

    SharedResource(const std::string& ownerName);

    SharedResource(ptr_t&& ptr, const std::string& ownerName);

    SharedResource(const ptr_t& ptr, const std::string& ownerName);

    SharedResource(const WeakResource<T>& ptr, const std::string& ownerName);

    SharedResource(const SharedResource<T>& copy, const std::string& ownerName);

    SharedResource(SharedResource<T>&& move) noexcept;

    ~SharedResource();

    SharedResource& set(const WeakResource<T>& weak, const std::string& name);

    SharedResource& set(const SharedResource<T>& copy, const std::string& name);

    SharedResource& operator=(SharedResource<T>&& move) noexcept;

    SharedResource& operator=(std::nullptr_t);

    void getAllReferenceOwnerNames(std::vector<std::string>& outOwnerNames) const;

    size_t use_count() const;

    void reset();

    ptr_t get() const noexcept;

    ptr_t operator->() const noexcept;

    ref_t operator*() const noexcept;

    bool operator==(const SharedResource<T>& other) const;

    bool operator==(std::nullptr_t) const;

    bool operator!=(const SharedResource<T>& other) const;

    bool operator!=(std::nullptr_t) const;

    bool operator>(const SharedResource<T>& other) const;

    bool operator<(const SharedResource<T>& other) const;

    explicit operator bool() const noexcept;

private:
    bool constructFromWeak(const WeakResource<T>& weak);

    void swap(SharedResource<T>& other);

    void incrRef();

    void decrRef();

    void registerOwner();

    void unregisterOwner();

private:
    std::string m_ownerName;
    Tracker* m_tracker;
    ptr_t m_ptr;
};


template<typename T>
class WeakResource {
    friend class SharedResource<T>;

    using Tracker = typename SharedResource<T>::Tracker;
    using ptr_t = typename SharedResource<T>::ptr_t;
    using ref_t = typename SharedResource<T>::ref_t;
public:
    WeakResource();

    WeakResource(const SharedResource<T>& shared);

    WeakResource(const WeakResource<T>& copy);

    WeakResource(WeakResource<T>&& move);

    ~WeakResource();

    WeakResource& operator=(const SharedResource<T>& shared);

    WeakResource& operator=(const WeakResource<T>& copy);

    WeakResource& operator=(WeakResource<T>&& move);

    size_t use_count() const;

    bool expired() const;

    SharedResource<T> lock(const std::string& ownerName) const;

    ptr_t get() const;

private:
    bool constructFromWeak(const WeakResource<T>& weak);

    void swap(WeakResource<T>& other);

    void incrRef();

    void decrRef();

private:
    Tracker* m_tracker;
    ptr_t m_ptr;
};


class GraphicsResource {
    NO_COPY(GraphicsResource);
public:
    enum ResourceType {
        ResourceType_None = 0,
        ResourceType_Mesh = 1,
        ResourceType_Buffer = 2,
        ResourceType_BufferView = 3,
        ResourceType_Texture = 4,
        ResourceType_Sampler = 5,
        ResourceType_Image2D = 6,
        ResourceType_ImageCube = 7,
        ResourceType_ImageView = 8,
        ResourceType_Framebuffer = 9,
        ResourceType_DescriptorPool = 10,
        ResourceType_DescriptorSetLayout = 11,
        ResourceType_DescriptorSet = 12,
        ResourceType_RenderPass = 13,
        ResourceType_GraphicsPipeline = 14,
        ResourceType_ComputePipeline = 15,
        ResourceType_CommandPool = 16,
        ResourceType_DeviceMemoryHeap = 17,
        ResourceType_Material = 18,
    };

public:
    GraphicsResource(const ResourceType& type, const WeakResource<vkr::Device>& device, const std::string& name);

    GraphicsResource(GraphicsResource&& move) noexcept;

    GraphicsResource& operator=(GraphicsResource&& move) noexcept;

    virtual ~GraphicsResource() = 0;

    const SharedResource<vkr::Device>& getDevice() const;

    const std::string& getName() const;

    const ResourceId& getResourceId() const;

    const ResourceType& getResourceType() const;

protected:
    SharedResource<vkr::Device> m_device;
    std::string m_name;

private:
    ResourceId m_resourceId;
    ResourceType m_resourceType;
};


template<typename T>
SharedResource<T>::Tracker::Tracker(const ptr_t& ptr):
        ptr(ptr),
        strongRefCount(1),
        weakRefCount(1) { // Self reference
}

template<typename T>
SharedResource<T>::Tracker::~Tracker() {
//    assert(strongRefCount == 0);
}

template<typename T>
bool SharedResource<T>::Tracker::incrNotZero() {
    if (strongRefCount != 0) {
        ++strongRefCount;
        return true;
    }
    return false;
}

template<typename T>
void SharedResource<T>::Tracker::incrStrong() {
    ++strongRefCount;
}

template<typename T>
void SharedResource<T>::Tracker::decrStrong() {
    assert(strongRefCount > 0);
    --strongRefCount;

    if (strongRefCount == 0) {
        delete ptr;
        ptr = nullptr;
        decrWeak();
    }
}

template<typename T>
void SharedResource<T>::Tracker::incrWeak() {
    ++weakRefCount;
}

template<typename T>
void SharedResource<T>::Tracker::decrWeak() {
    assert(weakRefCount > 0);
    --weakRefCount;
    if (weakRefCount == 0) {
        assert(strongRefCount == 0 && ptr == nullptr);
        delete this; // Object suicide. The last weak reference is itself, which was decremented upon the last strong reference reaching zero, therefor there are no external references and this is safe.
    }
}

template<typename T>
void SharedResource<T>::Tracker::getAllReferenceOwnerNames(std::vector<std::string>& outOwnerNames) const {
    for (const auto& [sharedRes, ownerName] : m_ownerNames) {
        outOwnerNames.emplace_back(ownerName);
    }
}

template<typename T>
SharedResource<T>::SharedResource():
        m_ownerName(std::string{}),
        m_tracker(nullptr),
        m_ptr(nullptr) {
}

template<typename T>
SharedResource<T>::SharedResource(std::nullptr_t):
        SharedResource() {
}

template<typename T>
SharedResource<T>::SharedResource(const std::string& ownerName):
        m_ownerName(ownerName),
        m_tracker(new Tracker(nullptr)),
        m_ptr(nullptr) {
    registerOwner();
}

template<typename T>
SharedResource<T>::SharedResource(ptr_t&& ptr, const std::string& ownerName):
        m_tracker(new Tracker(ptr)),
        m_ownerName(ownerName),
        m_ptr(ptr) {
    ptr = nullptr;
    registerOwner();
}

template<typename T>
SharedResource<T>::SharedResource(const ptr_t& ptr, const std::string& ownerName):
        m_tracker(new Tracker(ptr)),
        m_ownerName(ownerName),
        m_ptr(ptr) {
    registerOwner();
}

template<typename T>
SharedResource<T>::SharedResource(const SharedResource<T>& copy, const std::string& ownerName):
        m_ownerName(ownerName),
        m_tracker(copy.m_tracker),
        m_ptr(copy.m_ptr) {
    incrRef();
    registerOwner();
}

template<typename T>
SharedResource<T>::SharedResource(const WeakResource<T>& copy, const std::string& ownerName):
        m_ownerName(ownerName) {
    if (!constructFromWeak(copy))
        throw std::bad_weak_ptr{};
    registerOwner();
}

template<typename T>
SharedResource<T>::SharedResource(SharedResource<T>&& move) noexcept:
        m_ownerName(move.m_ownerName),
        m_tracker(std::exchange(move.m_tracker, nullptr)),
        m_ptr(std::exchange(move.m_ptr, nullptr)) {
    move.unregisterOwner();
    registerOwner();
}

template<typename T>
SharedResource<T>::~SharedResource() {
    unregisterOwner();
    decrRef();
}

template<typename T>
SharedResource<T>& SharedResource<T>::set(const WeakResource<T>& weak, const std::string& name) {
    SharedResource<T>(weak, name).swap(*this);
    return *this;
}

template<typename T>
SharedResource<T>& SharedResource<T>::set(const SharedResource<T>& copy, const std::string& name) {
    SharedResource<T>(copy, name).swap(*this);
    return *this;
}

template<typename T>
SharedResource<T>& SharedResource<T>::operator=(SharedResource<T>&& move) noexcept {
    SharedResource<T>(std::move(move)).swap(*this);
    return *this;
}


template<typename T>
SharedResource<T>& SharedResource<T>::operator=(std::nullptr_t) {
    SharedResource<T>(nullptr).swap(*this);
    return *this;
}

template<typename T>
void SharedResource<T>::getAllReferenceOwnerNames(std::vector<std::string>& outOwnerNames) const {
    if (m_tracker != nullptr)
        m_tracker->getAllReferenceOwnerNames(outOwnerNames);
}

template<typename T>
size_t SharedResource<T>::use_count() const {
    return m_tracker == nullptr ? 0 : (size_t)m_tracker->strongRefCount;
}

template<typename T>
void SharedResource<T>::reset() {
    SharedResource<T>().swap(*this);
}

template<typename T>
T* SharedResource<T>::get() const noexcept {
    return m_ptr;
}

template<typename T>
T* SharedResource<T>::operator->() const noexcept {
    return get();
}

template<typename T>
T& SharedResource<T>::operator*() const noexcept {
    return *get();
}

template<typename T>
bool SharedResource<T>::operator==(const SharedResource<T>& other) const {
    return get() == other.get();
}

template<typename T>
bool SharedResource<T>::operator==(std::nullptr_t) const {
    return get() == nullptr;
}

template<typename T>
bool SharedResource<T>::operator!=(const SharedResource<T>& other) const {
    return get() != other.get();
}

template<typename T>
bool SharedResource<T>::operator!=(std::nullptr_t) const {
    return get() != nullptr;
}

template<typename T>
bool SharedResource<T>::operator>(const SharedResource<T>& other) const {
    return get() < other.get();
}

template<typename T>
bool SharedResource<T>::operator<(const SharedResource<T>& other) const {
    return get() > other.get();
}

template<typename T>
SharedResource<T>::operator bool() const noexcept {
    return get() != nullptr;
}

template<typename T>
bool SharedResource<T>::constructFromWeak(const WeakResource<T>& weak) {
    if (weak.m_tracker != nullptr && weak.m_tracker->incrNotZero()) {
        m_tracker = weak.m_tracker;
        m_ptr = weak.m_ptr;
        return true;
    }
    return false;
}

template<typename T>
void SharedResource<T>::swap(SharedResource<T>& other) {
    std::swap(m_ownerName, other.m_ownerName);
    std::swap(m_tracker, other.m_tracker);
    std::swap(m_ptr, other.m_ptr);
}

template<typename T>
void SharedResource<T>::incrRef() {
    if (m_tracker != nullptr)
        m_tracker->incrStrong();
}

template<typename T>
void SharedResource<T>::decrRef() {
    if (m_tracker != nullptr)
        m_tracker->decrStrong();
}

template<typename T>
void SharedResource<T>::registerOwner() {
    if (m_tracker != nullptr) {
        m_tracker->m_ownerNames.insert(std::make_pair(this, m_ownerName));
    }
}

template<typename T>
void SharedResource<T>::unregisterOwner() {
    if (m_tracker != nullptr) {
        m_tracker->m_ownerNames.erase(this);
    }
}


template<typename T>
WeakResource<T>::WeakResource():
        m_tracker(nullptr),
        m_ptr(nullptr) {
}

template<typename T>
WeakResource<T>::WeakResource(const SharedResource<T>& shared):
        m_tracker(shared.m_tracker),
        m_ptr(shared.m_ptr) {
    incrRef();
}

template<typename T>
WeakResource<T>::WeakResource(const WeakResource<T>& copy) {
    constructFromWeak(copy);
}

template<typename T>
WeakResource<T>::WeakResource(WeakResource<T>&& move):
        m_tracker(std::exchange(move.m_tracker, nullptr)),
        m_ptr(std::exchange(move.m_ptr, nullptr)) {
}

template<typename T>
WeakResource<T>& WeakResource<T>::operator=(const SharedResource<T>& shared) {
    WeakResource<T>(shared).swap(*this);
    return *this;
}

template<typename T>
WeakResource<T>& WeakResource<T>::operator=(const WeakResource<T>& copy) {
    WeakResource<T>(copy).swap(*this);
    return *this;
}

template<typename T>
WeakResource<T>::~WeakResource() {
    decrRef();
}

template<typename T>
WeakResource<T>& WeakResource<T>::operator=(WeakResource<T>&& move) {
    WeakResource<T>(move).swap(*this);
    return *this;
}

template<typename T>
size_t WeakResource<T>::use_count() const {
    return m_tracker == nullptr ? 0 : (size_t)m_tracker->strongRefCount;
}

template<typename T>
bool WeakResource<T>::expired() const {
    return m_tracker == nullptr || m_tracker->strongRefCount == 0;
}

template<typename T>
SharedResource<T> WeakResource<T>::lock(const std::string& ownerName) const {
    SharedResource<T> shared(ownerName);
    shared.constructFromWeak(*this);
    return shared;
}


template<typename T>
typename WeakResource<T>::ptr_t WeakResource<T>::get() const {
    return m_ptr;
}

template<typename T>
bool WeakResource<T>::constructFromWeak(const WeakResource<T>& weak) {
    assert(weak.m_ptr != nullptr && weak.m_tracker->strongRefCount > 0);
    m_ptr = weak.m_ptr;
    m_tracker = weak.m_tracker;
    incrRef();
    return true;
}

template<typename T>
void WeakResource<T>::swap(WeakResource<T>& other) {
    std::swap(m_tracker, other.m_tracker);
    std::swap(m_ptr, other.m_ptr);
}

template<typename T>
void WeakResource<T>::incrRef() {
    if (m_tracker != nullptr)
        m_tracker->incrWeak();
}

template<typename T>
void WeakResource<T>::decrRef() {
    if (m_tracker != nullptr)
        m_tracker->decrWeak();
}

#endif //WORLDENGINE_GRAPHICSRESOURCE_H
