#ifndef WORLDENGINE_RENDERSTRESSTESTAPPLICATION_H
#define WORLDENGINE_RENDERSTRESSTESTAPPLICATION_H

#include "core/core.h"
#include "core/application/Application.h"
#include "core/engine/renderer/Material.h"
#include "core/graphics/Mesh.h"

class RenderStressTestApplication : public Application {
public:
    RenderStressTestApplication();

    virtual ~RenderStressTestApplication() override;

    virtual void init() override;

    virtual void cleanup() override;

    virtual void render(double dt) override;

    virtual void tick(double dt) override;

private:
    void handleUserInput(double dt);

private:
    double m_cameraPitch;
    double m_cameraYaw;
    double m_playerMovementSpeed;

    std::shared_ptr<Mesh> m_sphereMesh;
    std::shared_ptr<Material> m_sphereMaterial;
};


#endif //WORLDENGINE_RENDERSTRESSTESTAPPLICATION_H
