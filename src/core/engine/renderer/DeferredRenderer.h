#ifndef WORLDENGINE_DEFERREDRENDERER_H
#define WORLDENGINE_DEFERREDRENDERER_H

#include "core/core.h"

class RenderPass;

class DeferredRenderer {
public:
    DeferredRenderer();

    ~DeferredRenderer();

    void init();

    void render(double dt);

private:
    void createRenderPass();

private:
    std::shared_ptr<RenderPass> m_renderPass;
};


#endif //WORLDENGINE_DEFERREDRENDERER_H
