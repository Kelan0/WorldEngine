#pragma once

#include <iostream>
#include <assert.h>
#include <vector>
#include <array>
#include <stack>
#include <set>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <memory>
#include <utility>
#include <cstddef>
#include <typeinfo>
#include <typeindex>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_SWIZZLE 
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#define VULKAN_HPP_NO_SPACESHIP_OPERATOR 
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>

#include <stb_image.h>

#include "hash.h"

namespace vkr = vk::raii;

#define NO_COPY(ClassName) \
	ClassName(const ClassName&) = delete; \
	ClassName& operator=(const ClassName&) = delete;



#define INT_DIV_CEIL(num, denom) (((num) + (denom) - 1) / (denom))