
#ifndef WORLDENGINE_RENDERCOMPONENT_H
#define WORLDENGINE_RENDERCOMPONENT_H

#include "../../core.h"

class Texture2D;
class Mesh;

struct RenderComponent {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Texture2D> texture;

    RenderComponent& setMesh(const std::shared_ptr<Mesh>& mesh);

    RenderComponent& setTexture(const std::shared_ptr<Texture2D>& texture);
};


#endif //WORLDENGINE_RENDERCOMPONENT_H
