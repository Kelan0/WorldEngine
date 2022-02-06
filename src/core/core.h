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
#include <glm/glm.hpp>
#include <memory>

#define NO_COPY(ClassName) \
	ClassName(const ClassName&) = delete; \
	ClassName& operator=(const ClassName&) = delete;