#ifndef WORLDENGINE_SHADOWMAP_H
#define WORLDENGINE_SHADOWMAP_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/graphics/DescriptorSet.h"

class Image2D;
class Texture;
class Framebuffer;
class GraphicsPipeline;
class RenderPass;
class Buffer;
class DescriptorSet;
class DescriptorSetLayout;
class ImageView;

enum ShadowMapType : uint8_t {
    ShadowMapType_Simple = 0,
    ShadowMapType_Variance = 1,
};

struct GPUShadowMap {
    glm::mat4 viewProjectionMatrix;
    float cascadeStartZ;
    float cascadeEndZ;
    float _pad0;
    float _pad1;
};

class ShadowMap {
    friend class LightRenderer;
public:
    enum RenderType {
        RenderType_Standard = 0,
        RenderType_VarianceShadowMap = 1,
    };
    enum ShadowType {
        ShadowType_CascadedShadowMap = 0,
    };
public:
    ShadowMap(ShadowType shadowType, RenderType renderType);

    virtual ~ShadowMap() = default;

    virtual void update() = 0;

    const ShadowType& getShadowType() const;

    const RenderType& getRenderType() const;

    const glm::uvec2& getResolution() const;

    void setResolution(uint32_t width, uint32_t height);

protected:
    ShadowType m_shadowType;
    RenderType m_renderType;
    glm::uvec2 m_resolution;
    uint32_t m_index;
};


class CascadedShadowMap : public ShadowMap {
    friend class LightRenderer;
private:
    struct Cascade {
        // TODO: uhhhh... make this more memory efficient. Can we avoid allocating the raw depth image for VSM, can we share the intermediate image between multiple shadow maps?
        double cascadeSplitDistance = 0.0;
        ImageView* shadowDepthImageView = nullptr;
        ImageView* shadowVarianceImageView = nullptr;
        Image2D* shadowDepthImage = nullptr;
        Image2D* shadowVarianceImage = nullptr;
        Framebuffer* shadowMapFramebuffer = nullptr;
        uint32_t vsmUpdateDescriptorSet = 0;
        FrameResource<DescriptorSet> vsmBlurDescriptorSetX{};
        FrameResource<DescriptorSet> vsmBlurDescriptorSetY{};
        Image2D* vsmBlurIntermediateImage = nullptr;
        ImageView* vsmBlurIntermediateImageView = nullptr;
        glm::mat4 viewProjectionMatrix;
    };
public:
    CascadedShadowMap(RenderType renderType);

    virtual ~CascadedShadowMap() override;

    virtual void update() override;

    size_t getNumCascades() const;

    void setNumCascades(size_t numCascades);

    double getCascadeSplitDistance(size_t cascadeIndex);

    void setCascadeSplitDistance(size_t cascadeIndex, double distance);

private:
    const Framebuffer* getCascadeFramebuffer(size_t cascadeIndex);

    const ImageView* getCascadeShadowVarianceImageView(size_t cascadeIndex);

    const ImageView* getCascadeVsmBlurIntermediateImageView(size_t cascadeIndex);

    const DescriptorSet* getCascadeVsmBlurXDescriptorSet(size_t cascadeIndex);

    const DescriptorSet* getCascadeVsmBlurYDescriptorSet(size_t cascadeIndex);

    void updateCascade(Cascade& cascade);

    void destroyCascade(Cascade& cascade);

private:
    std::vector<Cascade> m_cascades;
    size_t m_numCascades;
};

#endif //WORLDENGINE_SHADOWMAP_H
