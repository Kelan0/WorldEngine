
#ifndef WORLDENGINE_CORE_H
#define WORLDENGINE_CORE_H

#include <iostream>
#include <assert.h>
#include <vector>
#include <array>
#include <stack>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <memory>
#include <utility>
#include <cstddef>
#include <typeinfo>
#include <typeindex>
#include <execution>
#include <future>
#include <cmath>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#define VULKAN_HPP_NO_SPACESHIP_OPERATOR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>

#include <stb_image.h>

#define ENTT_ID_TYPE uint64_t
#define ENTT_DISABLE_ASSERT 1

#include "hash.h"
#include "util/Exception.h"

namespace vkr = vk::raii;

#define NO_COPY(ClassName) \
	ClassName(const ClassName&) = delete; \
	ClassName& operator=(const ClassName&) = delete;

#define NO_MOVE(ClassName) \
    ClassName(ClassName&&) = delete;

#define INT_DIV_CEIL(num, denom) (((num) + (denom) - 1) / (denom))

constexpr uint32_t CONCURRENT_FRAMES = 3;

typedef uint64_t GraphicsResource;

#endif //WORLDENGINE_CORE_H
