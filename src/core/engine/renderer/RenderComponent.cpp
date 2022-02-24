#include "RenderComponent.h"

#include "../../graphics/ShaderProgram.h"

RenderComponent& RenderComponent::setMesh(const std::shared_ptr<Mesh>& mesh) {
    this->mesh = mesh;
    return *this;
}

RenderComponent& RenderComponent::setTexture(const std::shared_ptr<Texture2D>& texture) {
    this->texture = texture;
    return *this;
}
