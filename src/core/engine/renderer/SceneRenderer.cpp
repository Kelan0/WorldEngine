#include "SceneRenderer.h"

SceneRenderer::SceneRenderer() {
}

SceneRenderer::~SceneRenderer() {
}

void SceneRenderer::render(double dt) {
}

void SceneRenderer::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* SceneRenderer::getScene() const {
    return m_scene;
}
