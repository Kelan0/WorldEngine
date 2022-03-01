#pragma once

#include "../core.h"

namespace DebugUtils {
	struct RenderInfo {
		size_t renderedPolygons = 0;
		size_t renderedVertices = 0;
		size_t renderedIndices = 0;
		size_t drawCalls = 0;
		size_t drawInstances = 0;
		uint64_t elapsedDrawNanosCPU = 0;

		void reset();

		RenderInfo& operator+=(const RenderInfo& rhs);
	};
}