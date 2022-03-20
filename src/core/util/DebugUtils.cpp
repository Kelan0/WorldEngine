#include "core/util/DebugUtils.h"

void DebugUtils::RenderInfo::reset() {
    renderedPolygons = 0;
    renderedVertices = 0;
    renderedIndices = 0;
    drawCalls = 0;
    drawInstances = 0;
    elapsedDrawNanosCPU = 0;
}

DebugUtils::RenderInfo& DebugUtils::RenderInfo::operator+=(const RenderInfo& rhs) {
    renderedPolygons += rhs.renderedPolygons;
    renderedVertices += rhs.renderedVertices;
    renderedIndices += rhs.renderedIndices;
    drawCalls += rhs.drawCalls;
    drawInstances += rhs.drawInstances;
    elapsedDrawNanosCPU += rhs.elapsedDrawNanosCPU;
    return *this;
}
