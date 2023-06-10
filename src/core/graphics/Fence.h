
#ifndef WORLDENGINE_FENCE_H
#define WORLDENGINE_FENCE_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"

struct FenceConfiguration {
    WeakResource<vkr::Device> device;
    bool createSignaled = false;
};

class Fence : public GraphicsResource {
    NO_COPY(Fence)
public:
    enum Status {
        Status_NotSignaled = 0,
        Status_Signaled = 1,
        Status_Error = 2,
    };
private:
    Fence(const WeakResource<vkr::Device>& device, const vk::Fence& fence, const std::string& name);

public:
    ~Fence() override;

    static Fence* create(const FenceConfiguration& fenceConfiguration, const std::string& name);

    const vk::Fence& getFence() const;

    Status getStatus() const;

    bool reset() const;

    bool wait(uint64_t timeout) const;

    static Status getFenceStatus(const vk::Device& device, const vk::Fence& fence);

    static Status getFenceStatus(const Fence* fence);

    static bool resetFences(const vk::Device& device, const vk::Fence* fences, uint32_t count);

    static bool resetFences(Fence* const* fences, uint32_t count);

    template<typename Iter>
    static bool resetFences(const vk::Device& device, Iter first, Iter last);

    static bool waitForFences(const vk::Device& device, const vk::Fence* fences, uint32_t count, bool waitForAll, uint64_t timeout);

    static bool waitForFences(Fence* const* fences, uint32_t count, bool waitForAll, uint64_t timeout);

    template<typename Iter>
    static bool waitForFences(const vk::Device& device, Iter first, Iter last, bool waitForAll, uint64_t timeout);

private:
    vk::Fence m_fence;
};



template<typename Iter>
bool Fence::resetFences(const vk::Device& device, Iter first, Iter last) {
    using value_type = typename std::iterator_traits<Iter>::value_type;
    using diff_type = typename std::iterator_traits<Iter>::difference_type;
    constexpr bool is_contiguous = std::is_same_v<std::contiguous_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>;

    diff_type count = std::distance(first, last);
    if (count == 0)
        return true;

    if constexpr (is_contiguous && std::is_same_v<vk::Fence, value_type>) {
        return resetFences(device, *first, count);
    }

    std::vector<vk::Fence> vkFences(count);

    if constexpr (std::is_same_v<vk::Fence, value_type>) {
        for (diff_type i = (diff_type)0; first != last; ++first, ++i)
            vkFences[i] = *first;

    } else { // std::is_same_v<Fence*, Type>
        for (diff_type i = (diff_type)0; first != last; ++first, ++i)
            vkFences[i] = (*first)->getFence();
    }

    return resetFences(device, &vkFences[0], (uint32_t)count);
}

template<typename Iter>
bool Fence::waitForFences(const vk::Device& device, Iter first, Iter last, bool waitForAll, uint64_t timeout) {
    using value_type = typename std::iterator_traits<Iter>::value_type;
    using diff_type = typename std::iterator_traits<Iter>::difference_type;
    constexpr bool is_contiguous = std::is_same_v<std::contiguous_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>;

    diff_type count = std::distance(first, last);
    if (count == 0)
        return true;

    if constexpr (is_contiguous && std::is_same_v<vk::Fence, value_type>) {
        return waitForFences(device, *first, count, waitForAll, timeout);
    }

    std::vector<vk::Fence> vkFences(count);

    if constexpr (std::is_same_v<vk::Fence, value_type>) {
        for (diff_type i = (diff_type)0; first != last; ++first, ++i)
            vkFences[i] = *first;

    } else { // std::is_same_v<Fence*, Type>
        for (diff_type i = (diff_type)0; first != last; ++first, ++i)
            vkFences[i] = (*first)->getFence();
    }

    return waitForFences(device, &vkFences[0], (uint32_t)count, waitForAll, timeout);
}

#endif //WORLDENGINE_FENCE_H
