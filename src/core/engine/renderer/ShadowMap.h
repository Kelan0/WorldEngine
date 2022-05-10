#ifndef WORLDENGINE_SHADOWMAP_H
#define WORLDENGINE_SHADOWMAP_H

#include "core/core.h"

class Image2D;
class Texture;

class DirectionShadowMap {
public:
    DirectionShadowMap(const glm::uvec2& resolution);

    const glm::vec3& getDirection() const;

    const glm::uvec2& getResolution() const;

private:
    glm::vec3 m_direction;
    glm::uvec2 m_resolution;
    std::shared_ptr<Image2D> m_shadowMapImage;
    std::shared_ptr<Texture> m_shadowMapTexture;
    bool m_needsRecompute;
};


#endif //WORLDENGINE_SHADOWMAP_H
