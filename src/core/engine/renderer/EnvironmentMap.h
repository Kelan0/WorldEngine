
#ifndef WORLDENGINE_ENVIRONMENTMAP_H
#define WORLDENGINE_ENVIRONMENTMAP_H

#include "core/core.h"
#include "core/engine/scene/event/Events.h"

class ImageCube;
class ImageView;
class Texture;
class DescriptorSet;
class ComputePipeline;
class Buffer;

class EnvironmentMap {
public:
    EnvironmentMap(const uint32_t& irradianceMapSize = 32, const uint32_t& specularMapSize = 256);

    ~EnvironmentMap();

    void update();

    void setEnvironmentImage(const std::shared_ptr<ImageCube>& environmentImage);

    const std::shared_ptr<ImageCube>& getEnvironmentImage() const;

    const std::shared_ptr<ImageCube>& getDiffuseIrradianceImage() const;

    const std::shared_ptr<ImageCube>& getSpecularReflectionImage() const;

    const std::shared_ptr<Texture>& getEnvironmentMapTexture() const;

    const std::shared_ptr<Texture>& getDiffuseIrradianceMapTexture() const;

    const std::shared_ptr<Texture>& getSpecularReflectionMapTexture() const;

private:
    void calculateDiffuseIrradiance(const vk::CommandBuffer& commandBuffer) const;

    void calculateSpecularReflection(const vk::CommandBuffer& commandBuffer) const;

    std::shared_ptr<Texture> createTexture(const std::shared_ptr<ImageCube>& image) const;

    static DescriptorSet* getDiffuseIrradianceComputeDescriptorSet();

    static ComputePipeline* getDiffuseIrradianceComputePipeline();

    static Buffer* getUniformBuffer(const vk::DeviceSize& size);

    static void onCleanupGraphics(const ShutdownGraphicsEvent& event);

private:
    uint32_t m_irradianceMapSize;
    uint32_t m_specularMapSize;

    std::shared_ptr<ImageCube> m_environmentImage;
    std::shared_ptr<ImageCube> m_diffuseIrradianceImage;
    std::shared_ptr<ImageCube> m_specularReflectionImage;

    std::shared_ptr<Texture> m_environmentMapTexture;
    std::shared_ptr<Texture> m_diffuseIrradianceMapTexture;
    std::shared_ptr<Texture> m_specularReflectionMapTexture;

    bool m_needsRecompute;

    static ComputePipeline* s_diffuseIrradianceConvolutionComputePipeline;
    static DescriptorSet* s_diffuseIrradianceConvolutionDescriptorSet;
    static Buffer* s_uniformBuffer;
};


#endif //WORLDENGINE_ENVIRONMENTMAP_H
