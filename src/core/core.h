#pragma once

#include <iostream>
#include <assert.h>
#include <vector>
#include <array>
#include <set>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <memory>
#include <utility>

//#define GLM_SWIZZLE 
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define VULKAN_HPP_NO_SPACESHIP_OPERATOR 
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>

#include <stb_image.h>

namespace vkr = vk::raii;

#define NO_COPY(ClassName) \
	ClassName(const ClassName&) = delete; \
	ClassName& operator=(const ClassName&) = delete;


// Why isn't this included in the standard library?
namespace std {
	template<class T>
	static inline void hash_combine(std::size_t& s, const T& v) {
		std::hash<T> h;
		s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
	}
};