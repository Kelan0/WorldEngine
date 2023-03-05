#ifndef WORLDENGINE_BLOOMTESTAPPLICATION_H
#define WORLDENGINE_BLOOMTESTAPPLICATION_H

#include "core/core.h"
#include "core/application/Application.h"

class Texture;
class Image2D;
class Sampler;
class Frustum;


class BloomTestApplication : public Application{
public:
    BloomTestApplication();

    ~BloomTestApplication() override;

    virtual void init() override;

    virtual void cleanup() override;

    virtual void render(const double& dt) override;

    virtual void tick(const double& dt) override;

private:
    void handleUserInput(const double& dt);

    std::shared_ptr<Texture> loadTexture(const std::string& filePath, vk::Format format, std::weak_ptr<Sampler> sampler);

private:
    std::vector<Image2D*> images;
    std::vector<Sampler*> samplers;
    std::vector<std::shared_ptr<Texture>> textures;
    double cameraPitch = 0.0F;
    double cameraYaw = 0.0F;
    float playerMovementSpeed = 1.0F;
    bool cameraMouseInputLocked = false;
    Frustum* frustum;
    bool pauseFrustum = false;
    float framerateLimit = 0.0F;
    bool histogramNormalized = true;
    float test = 1.0F;
    double time = 0.0;
};


#endif //WORLDENGINE_BLOOMTESTAPPLICATION_H
