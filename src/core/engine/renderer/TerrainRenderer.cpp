
#include "TerrainRenderer.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/util/Logger.h"

TerrainRenderer::TerrainRenderer() {

}

TerrainRenderer::~TerrainRenderer() {

}

bool TerrainRenderer::init() {
    LOG_INFO("Initializing TerrainRenderer");
    return true;
}

void TerrainRenderer::preRender(double dt) {

}

void TerrainRenderer::render(double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum) {

}