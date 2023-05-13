
#ifndef WORLDENGINE_ENVIRONMENTMAP_H
#define WORLDENGINE_ENVIRONMENTMAP_H

#include "core/core.h"

class Image2D;
class ImageCube;
class ImageView;
class Texture;
class Sampler;
class DescriptorSet;
class ComputePipeline;
class Buffer;
struct ShutdownGraphicsEvent;

class EnvironmentMap {
public:
    explicit EnvironmentMap(uint32_t irradianceMapSize = 32, uint32_t specularMapSize = 256);

    ~EnvironmentMap();

    void update();

    void setEmptyEnvironmentImage();

    void setEnvironmentImage(const std::shared_ptr<ImageCube>& environmentImage);

    const std::shared_ptr<ImageCube>& getEnvironmentImage() const;

    const std::shared_ptr<ImageCube>& getDiffuseIrradianceImage() const;

    const std::shared_ptr<ImageCube>& getSpecularReflectionImage() const;

    const std::shared_ptr<Texture>& getEnvironmentMapTexture() const;

    const std::shared_ptr<Texture>& getDiffuseIrradianceMapTexture() const;

    const std::shared_ptr<Texture>& getSpecularReflectionMapTexture() const;

    static std::shared_ptr<Texture> getBRDFIntegrationMap(const vk::CommandBuffer& commandBuffer = nullptr);

private:
    void calculateDiffuseIrradiance(const vk::CommandBuffer& commandBuffer) const;

    void calculateSpecularReflection(const vk::CommandBuffer& commandBuffer) const;

    static void calculateBRDFIntegrationMap(const vk::CommandBuffer& commandBuffer);

    std::shared_ptr<Texture> createTexture(const std::shared_ptr<ImageCube>& image, uint32_t baseMipLevel, const uint32_t mipLevelCount) const;

    static DescriptorSet* getDiffuseIrradianceComputeDescriptorSet();

    static ComputePipeline* getDiffuseIrradianceComputePipeline();

    static DescriptorSet* getPrefilteredEnvironmentComputeDescriptorSet();

    static ComputePipeline* getPrefilteredEnvironmentComputePipeline();

    static Buffer* getUniformBuffer();

    static void onCleanupGraphics(ShutdownGraphicsEvent* event);

private:
    uint32_t m_irradianceMapSize;
    uint32_t m_specularMapSize;
    uint32_t m_specularMapMipLevels;

    std::shared_ptr<ImageCube> m_environmentImage;
    std::shared_ptr<ImageCube> m_diffuseIrradianceImage;
    std::shared_ptr<ImageCube> m_specularReflectionImage;

    std::shared_ptr<Texture> m_environmentMapTexture;
    std::shared_ptr<Texture> m_diffuseIrradianceMapTexture;
    std::shared_ptr<Texture> m_specularReflectionMapTexture;
    std::vector<std::shared_ptr<Texture>> m_specularReflectionMapTextureMipLevels;

    bool m_needsRecompute;

    static ComputePipeline* s_diffuseIrradianceConvolutionComputePipeline;
    static DescriptorSet* s_diffuseIrradianceConvolutionDescriptorSet;
    static ComputePipeline* s_prefilteredEnvironmentComputePipeline;
    static DescriptorSet* s_prefilteredEnvironmentDescriptorSet;
    static ComputePipeline* s_BRDFIntegrationMapComputePipeline;
    static DescriptorSet* s_BRDFIntegrationMapDescriptorSet;
    static Buffer* s_uniformBuffer;

    static std::shared_ptr<Image2D> s_BRDFIntegrationMapImage;
    static std::shared_ptr<Texture> s_BRDFIntegrationMap;
};


#endif //WORLDENGINE_ENVIRONMENTMAP_H
