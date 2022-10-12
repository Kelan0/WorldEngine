
#include "core/engine/renderer/ShadowMap.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/renderPasses/LightRenderer.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/Texture.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/util/Profiler.h"


ShadowMap::ShadowMap(const uint32_t& width, const uint32_t& height):
        ShadowMap(glm::uvec2(width, height)) {
}

ShadowMap::ShadowMap(const glm::uvec2& resolution):
        m_index(-1),
        m_resolution(resolution),
        m_shadowDepthImage(nullptr),
        m_shadowVarianceImage(nullptr),
        m_shadowDepthImageView(nullptr),
        m_shadowVarianceImageView(nullptr),
        m_shadowMapFramebuffer(nullptr) {
}

ShadowMap::~ShadowMap() {
    delete m_shadowMapFramebuffer;
    delete m_shadowDepthImageView;
    delete m_shadowVarianceImageView;
    delete m_shadowDepthImage;
    delete m_shadowVarianceImage;
}

void ShadowMap::update() {
    PROFILE_SCOPE("ShadowMap::update");

    if (m_shadowVarianceImage == nullptr || m_shadowVarianceImage->getWidth() != m_resolution.x || m_shadowVarianceImage->getHeight() != m_resolution.y) {
        delete m_shadowDepthImageView;
        delete m_shadowVarianceImageView;
        delete m_shadowDepthImage;
        delete m_shadowVarianceImage;

        Image2DConfiguration imageConfig;
        imageConfig.device = Engine::graphics()->getDevice();
        imageConfig.setSize(m_resolution);
        imageConfig.mipLevels = 1;
        imageConfig.generateMipmap = false;
        imageConfig.sampleCount = vk::SampleCountFlagBits::e1;

        imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
        imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage;
        m_shadowVarianceImage = Image2D::create(imageConfig);
        assert(m_shadowVarianceImage != nullptr);

        imageConfig.format = vk::Format::eD32Sfloat;
        imageConfig.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        m_shadowDepthImage = Image2D::create(imageConfig);
        assert(m_shadowDepthImage != nullptr);

        ImageViewConfiguration imageViewConfig;
        imageViewConfig.device = Engine::graphics()->getDevice();

        imageViewConfig.setImage(m_shadowVarianceImage);
        imageViewConfig.format = vk::Format::eR32G32B32A32Sfloat;
        imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
        m_shadowVarianceImageView = ImageView::create(imageViewConfig);
        assert(m_shadowVarianceImageView != nullptr);

        imageViewConfig.setImage(m_shadowDepthImage);
        imageViewConfig.format = vk::Format::eD32Sfloat;
        imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eDepth;
        m_shadowDepthImageView = ImageView::create(imageViewConfig);
        assert(m_shadowDepthImageView != nullptr);

    }

    if (m_shadowMapFramebuffer == nullptr || m_shadowMapFramebuffer->getWidth() != m_resolution.x || m_shadowMapFramebuffer->getHeight() != m_resolution.y) {
        FramebufferConfiguration framebufferConfig;
        framebufferConfig.device = Engine::graphics()->getDevice();
        framebufferConfig.setSize(m_resolution);
        framebufferConfig.setRenderPass(Engine::lightRenderer()->getRenderPass().get());
        framebufferConfig.addAttachment(m_shadowVarianceImageView);
        framebufferConfig.addAttachment(m_shadowDepthImageView);

        delete m_shadowMapFramebuffer;
        m_shadowMapFramebuffer = Framebuffer::create(framebufferConfig);
        assert(!!m_shadowMapFramebuffer);
    }
}

void ShadowMap::beginRenderPass(const vk::CommandBuffer& commandBuffer, const RenderPass* renderPass) {
    renderPass->begin(commandBuffer, m_shadowMapFramebuffer, vk::SubpassContents::eInline);
}

const uint32_t &ShadowMap::getIndex() const {
    return m_index;
}

void ShadowMap::setResolution(const glm::uvec2& resolution) {
    m_resolution = resolution;
}

const glm::uvec2& ShadowMap::getResolution() const {
    return m_resolution;
}

const ImageView* ShadowMap::getShadowVarianceImageView() const {
    return m_shadowVarianceImageView;
}

const glm::mat4& ShadowMap::getViewProjectionMatrix() const {
    return m_viewProjectionMatrix;
}
