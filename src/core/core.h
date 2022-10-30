
#ifndef WORLDENGINE_CORE_H
#define WORLDENGINE_CORE_H

#include <iostream>
#include <cassert>
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
#include <glm/gtc/packing.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>

//#define USE_VOLK

#define VULKAN_HPP_NO_SPACESHIP_OPERATOR

#ifdef USE_VOLK
#define VK_NO_PROTOTYPES
#include "core/volk.h"
#else
#include "Vulkan.h"
#endif

#include <vulkan/vulkan_raii.hpp>

#include "extern/stbi/stb_image.h"

#define ENTT_ID_TYPE uint64_t
#define ENTT_DISABLE_ASSERT 1

#include "hash.h"
#include "util/Exception.h"
#include "core/graphics/GraphicsResource.h"

namespace vkr = vk::raii;

#define NO_COPY(ClassName) \
	ClassName(const ClassName&) = delete; \
	ClassName& operator=(const ClassName&) = delete;

#define NO_MOVE(ClassName) \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete;

#define INT_DIV_CEIL(num, denom) (((num) + (denom) - 1) / (denom))
#define CEIL_TO_MULTIPLE(value, multiple) (INT_DIV_CEIL(value, multiple) * multiple)
#define FLOOR_TO_MULTIPLE(value, multiple) ((value / multiple) * multiple)

#ifndef ENABLE_SHADER_HOT_RELOAD
#define ENABLE_SHADER_HOT_RELOAD 1
#endif

constexpr uint32_t CONCURRENT_FRAMES = 3;

typedef uint64_t GraphicsResource;

#endif //WORLDENGINE_CORE_H
