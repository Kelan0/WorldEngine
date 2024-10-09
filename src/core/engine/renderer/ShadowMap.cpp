
#include "core/engine/renderer/ShadowMap.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/renderPasses/LightRenderer.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Texture.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/util/Profiler.h"


ShadowMap::ShadowMap(ShadowType shadowType, RenderType renderType):
        m_shadowType(shadowType),
        m_renderType(renderType),
        m_resolution(0, 0),
        m_index(0) {
}

const ShadowMap::ShadowType& ShadowMap::getShadowType() const {
    return m_shadowType;
}

const ShadowMap::RenderType& ShadowMap::getRenderType() const {
    return m_renderType;
}

const glm::uvec2& ShadowMap::getResolution() const {
    return m_resolution;
}

void ShadowMap::setResolution(uint32_t width, uint32_t height) {
    m_resolution.x = width;
    m_resolution.y = height;
}




CascadedShadowMap::CascadedShadowMap(RenderType renderType):
        ShadowMap(ShadowType_CascadedShadowMap, renderType),
        m_numCascades(4) {
}

CascadedShadowMap::~CascadedShadowMap() {
    for (size_t i = 0; i < m_cascades.size(); ++i) {
        destroyCascade(m_cascades[i]);
    }
}

void CascadedShadowMap::update() {
    for (size_t i = 0; i < m_numCascades; ++i) {
        updateCascade(m_cascades[i]);
    }

    if (m_cascades.size() > m_numCascades) {
        for (size_t i = m_numCascades; i < m_cascades.size(); ++i) {
            destroyCascade(m_cascades[i]);
        }
        m_cascades.resize(m_numCascades);
    }
}

size_t CascadedShadowMap::getNumCascades() const {
    return m_numCascades;
}

void CascadedShadowMap::setNumCascades(size_t numCascades) {
    m_numCascades = numCascades;
    if (m_cascades.size() <= numCascades) {
        m_cascades.resize(numCascades);
    }
}

double CascadedShadowMap::getCascadeSplitDistance(size_t cascadeIndex) {
    assert(cascadeIndex < m_cascades.size());
    return m_cascades[cascadeIndex].cascadeSplitDistance;
}

void CascadedShadowMap::setCascadeSplitDistance(size_t cascadeIndex, double distance) {
    assert(cascadeIndex < m_cascades.size());
    m_cascades[cascadeIndex].cascadeSplitDistance = distance;
}

const Framebuffer* CascadedShadowMap::getCascadeFramebuffer(size_t cascadeIndex) {
    assert(cascadeIndex < m_cascades.size());
    return m_cascades[cascadeIndex].shadowMapFramebuffer;
}

const ImageView* CascadedShadowMap::getCascadeShadowDepthImageView(size_t cascadeIndex) {
    assert(cascadeIndex < m_cascades.size());
    return m_cascades[cascadeIndex].shadowDepthImageView;
}

const ImageView* CascadedShadowMap::getCascadeShadowVarianceImageView(size_t cascadeIndex) {
    assert(cascadeIndex < m_cascades.size());
    return m_cascades[cascadeIndex].shadowVarianceImageView;
}

const ImageView* CascadedShadowMap::getCascadeVsmBlurIntermediateImageView(size_t cascadeIndex) {
    assert(cascadeIndex < m_cascades.size());
    return m_cascades[cascadeIndex].vsmBlurIntermediateImageView;
}

const DescriptorSet* CascadedShadowMap::getCascadeVsmBlurXDescriptorSet(size_t cascadeIndex) {
    assert(cascadeIndex < m_cascades.size());
    return m_cascades[cascadeIndex].vsmBlurDescriptorSetX.get();
}

const DescriptorSet* CascadedShadowMap::getCascadeVsmBlurYDescriptorSet(size_t cascadeIndex) {
    assert(cascadeIndex < m_cascades.size());
    return m_cascades[cascadeIndex].vsmBlurDescriptorSetY.get();
}

void CascadedShadowMap::updateCascade(Cascade& cascade) {
    if (m_renderType == RenderType_VarianceShadowMap) {

        if (cascade.vsmBlurDescriptorSetX == nullptr) {
            cascade.vsmBlurDescriptorSetX = DescriptorSet::create(Engine::instance()->getLightRenderer()->getVsmBlurComputeDescriptorSetLayout(), Engine::graphics()->descriptorPool(),"CascadedShadowMap-VsmBlurXComputeDescriptorSet");
            cascade.vsmUpdateDescriptorSet++;
        }

        if (cascade.vsmBlurDescriptorSetY == nullptr) {
            cascade.vsmBlurDescriptorSetY = DescriptorSet::create(Engine::instance()->getLightRenderer()->getVsmBlurComputeDescriptorSetLayout(), Engine::graphics()->descriptorPool(),"CascadedShadowMap-VsmBlurYComputeDescriptorSet");
            cascade.vsmUpdateDescriptorSet++;
        }

        bool recreatedImages = false;
        if (cascade.shadowVarianceImage == nullptr || cascade.shadowVarianceImage->getWidth() != m_resolution.x || cascade.shadowVarianceImage->getHeight() != m_resolution.y) {
            recreatedImages = true;

            delete cascade.shadowDepthImageView;
            delete cascade.shadowVarianceImageView;
            delete cascade.vsmBlurIntermediateImageView;
            delete cascade.shadowDepthImage;
            delete cascade.shadowVarianceImage;
            delete cascade.vsmBlurIntermediateImage;

            Image2DConfiguration imageConfig{};
            imageConfig.device = Engine::graphics()->getDevice();
            imageConfig.setSize(m_resolution);
            imageConfig.mipLevels = 1;
            imageConfig.generateMipmap = false;
            imageConfig.sampleCount = vk::SampleCountFlagBits::e1;
            imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

            imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
            imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage;
            cascade.shadowVarianceImage = Image2D::create(imageConfig, "ShadowMap-ShadowVarianceImage");
            assert(cascade.shadowVarianceImage != nullptr);

            imageConfig.format = vk::Format::eD32Sfloat;
            imageConfig.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
            cascade.shadowDepthImage = Image2D::create(imageConfig, "ShadowMap-ShadowDepthImage");
            assert(cascade.shadowDepthImage != nullptr);

            imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
            imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
            cascade.vsmBlurIntermediateImage = Image2D::create(imageConfig, "ShadowMap-VsmBlurIntermediateImage");
            assert(cascade.vsmBlurIntermediateImage != nullptr);

            ImageViewConfiguration imageViewConfig{};
            imageViewConfig.device = Engine::graphics()->getDevice();

            imageViewConfig.setImage(cascade.shadowVarianceImage);
            imageViewConfig.format = vk::Format::eR32G32B32A32Sfloat;
            imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
            cascade.shadowVarianceImageView = ImageView::create(imageViewConfig, "ShadowMap-ShadowVarianceImageView");
            assert(cascade.shadowVarianceImageView != nullptr);

            imageViewConfig.setImage(cascade.shadowDepthImage);
            imageViewConfig.format = vk::Format::eD32Sfloat;
            imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eDepth;
            cascade.shadowDepthImageView = ImageView::create(imageViewConfig, "ShadowMap-ShadowDepthImageView");
            assert(cascade.shadowDepthImageView != nullptr);

            imageViewConfig.setImage(cascade.vsmBlurIntermediateImage);
            imageViewConfig.format = vk::Format::eR32G32B32A32Sfloat;
            imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
            cascade.vsmBlurIntermediateImageView = ImageView::create(imageViewConfig, "ShadowMap-VsmBlurIntermediateImageView");
            assert(cascade.vsmBlurIntermediateImageView != nullptr);

            cascade.vsmUpdateDescriptorSet = CONCURRENT_FRAMES;
        }

        if (cascade.vsmUpdateDescriptorSet > 0) {
            cascade.vsmUpdateDescriptorSet--;

            DescriptorSetWriter(cascade.vsmBlurDescriptorSetX.get())
                    .writeImage(0, Engine::instance()->getLightRenderer()->getVsmShadowMapSampler().get(), cascade.shadowVarianceImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                    .writeImage(1, Engine::instance()->getLightRenderer()->getVsmShadowMapSampler().get(), cascade.vsmBlurIntermediateImageView, vk::ImageLayout::eGeneral, 0, 1)
                    .write();

            DescriptorSetWriter(cascade.vsmBlurDescriptorSetY.get())
                    .writeImage(0, Engine::instance()->getLightRenderer()->getVsmShadowMapSampler().get(), cascade.vsmBlurIntermediateImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                    .writeImage(1, Engine::instance()->getLightRenderer()->getVsmShadowMapSampler().get(), cascade.shadowVarianceImageView, vk::ImageLayout::eGeneral, 0, 1)
                    .write();
        }

        if (cascade.shadowMapFramebuffer == nullptr || recreatedImages || cascade.shadowMapFramebuffer->getWidth() != m_resolution.x || cascade.shadowMapFramebuffer->getHeight() != m_resolution.y) {
            delete cascade.shadowMapFramebuffer;

            FramebufferConfiguration framebufferConfig{};
            framebufferConfig.device = Engine::graphics()->getDevice();
            framebufferConfig.setSize(m_resolution);
            framebufferConfig.setRenderPass(Engine::instance()->getLightRenderer()->getRenderPass().get());
            framebufferConfig.addAttachment(cascade.shadowVarianceImageView);
            framebufferConfig.addAttachment(cascade.shadowDepthImageView);

            cascade.shadowMapFramebuffer = Framebuffer::create(framebufferConfig, "ShadowMap-ShadowMapFramebuffer");
            assert(cascade.shadowMapFramebuffer != nullptr);
        }
    }
}

void CascadedShadowMap::destroyCascade(Cascade& cascade) {
    delete cascade.shadowDepthImageView;
    delete cascade.shadowVarianceImageView;
    delete cascade.shadowDepthImage;
    delete cascade.shadowVarianceImage;
    delete cascade.shadowMapFramebuffer;
    delete cascade.vsmBlurIntermediateImage;
    delete cascade.vsmBlurIntermediateImageView;
    cascade.shadowDepthImageView = nullptr;
    cascade.shadowVarianceImageView = nullptr;
    cascade.shadowDepthImage = nullptr;
    cascade.shadowVarianceImage = nullptr;
    cascade.shadowMapFramebuffer = nullptr;
    cascade.vsmBlurIntermediateImage = nullptr;
    cascade.vsmBlurIntermediateImageView = nullptr;
}