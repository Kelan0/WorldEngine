#ifndef WORLDENGINE_TERRAINTESTAPPLICATION_H
#define WORLDENGINE_TERRAINTESTAPPLICATION_H

#include "core/core.h"
#include "core/application/Application.h"

class TerrainTestApplication : public Application {
public:
    TerrainTestApplication();

    virtual ~TerrainTestApplication() override;

    virtual void init() override;

    virtual void cleanup() override;

    virtual void render(double dt) override;

    virtual void tick(double dt) override;

    void handleUserInput(double dt);

private:
    double cameraPitch = 0.0F;
    double cameraYaw = 0.0F;
    float playerMovementSpeed = 4.0F;
    float targetZoomFactor = 1.0F;
    float currentZoomFactor = 1.0F;
};


#endif //WORLDENGINE_TERRAINTESTAPPLICATION_H
