
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/application/Engine.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/Mesh.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"
#include "core/util/Logger.h"


ImmediateRenderer::ColouredVertex::ColouredVertex():
    position(0.0F, 0.0F, 0.0F),
    normal(0.0F, 0.0F, 0.0F),
    texture(0.0F, 0.0F),
    colour(255, 255, 255, 255) {
}

ImmediateRenderer::ColouredVertex::ColouredVertex(const ColouredVertex& copy):
    position(copy.position),
    normal(copy.normal),
    texture(copy.texture),
    colour(copy.colour) {
}

ImmediateRenderer::ColouredVertex::ColouredVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture, const glm::u8vec4& colour):
        position(position),
        normal(normal),
        texture(texture),
        colour(colour) {
}

ImmediateRenderer::ColouredVertex::ColouredVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture, const glm::vec4& colour):
        position(position),
        normal(normal),
        texture(texture),
        colour(glm::clamp(glm::u8vec4(colour * 255.0F), (uint8_t)0, (uint8_t)255)) {
}

ImmediateRenderer::ColouredVertex::ColouredVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, uint8_t r, uint8_t g, uint8_t b, uint8_t a):
        ColouredVertex(glm::vec3(px, py, pz), glm::vec3(nx, ny, nz), glm::vec2(tx, ty), glm::u8vec4(r, g, b, a)) {
}

ImmediateRenderer::ColouredVertex::ColouredVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, float r, float g, float b, float a):
        ColouredVertex(glm::vec3(px, py, pz), glm::vec3(nx, ny, nz), glm::vec2(tx, ty), glm::vec4(r, g, b, a)) {
}


bool ImmediateRenderer::ColouredVertex::equalsEpsilon(const ColouredVertex& vertex, float epsilon) const {
    if (glm::abs(position.x - vertex.position.x) >= epsilon) return false;
    if (glm::abs(position.y - vertex.position.y) >= epsilon) return false;
    if (glm::abs(position.z - vertex.position.z) >= epsilon) return false;
    if (glm::abs(normal.x - vertex.normal.x) >= epsilon) return false;
    if (glm::abs(normal.y - vertex.normal.y) >= epsilon) return false;
    if (glm::abs(normal.z - vertex.normal.z) >= epsilon) return false;
    if (glm::abs(texture.x - vertex.texture.x) >= epsilon) return false;
    if (glm::abs(texture.y - vertex.texture.y) >= epsilon) return false;
    if (rgba != vertex.rgba) return false;
    return true;
}


ImmediateRenderer::ImmediateRenderer():
        m_currentCommand(nullptr),
        m_matrixMode(MatrixMode_ModelView),
        m_normal(0, 0, 0),
        m_texture(0, 0),
        m_colour(0, 0, 0, 0),
        m_vertexCount(0),
        m_indexCount(0),
        m_firstChangedVertex(0),
        m_firstChangedIndex(0) {
    m_modelMatrixStack.emplace(1.0F); // implicit identity matrix
    m_projectionMatrixStack.emplace(1.0F); // implicit identity matrix

    m_resources.initDefault();

    colour(glm::u8vec4(255, 255, 255, 255));
    normal(glm::vec3(0.0F, 0.0F, 0.0F));
    texture(glm::vec2(0.0F, 0.0F));
}

ImmediateRenderer::~ImmediateRenderer() {
    LOG_INFO("Destroying ImmediateRenderer");
    for (auto it = m_graphicsPipelines.begin(); it != m_graphicsPipelines.end(); ++it)
        delete it->second;

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        if (m_resources[i] != nullptr) {
            delete m_resources[i]->descriptorSet;
            delete m_resources[i]->vertexBuffer;
            delete m_resources[i]->indexBuffer;
            delete m_resources[i]->uniformBuffer;
            delete m_resources[i]->framebuffer;
            delete m_resources[i]->frameColourImageView;
            delete m_resources[i]->frameColourImage;
            delete m_resources[i]->frameDepthImageView;
            delete m_resources[i]->frameDepthImage;
        }
    }

    Engine::eventDispatcher()->disconnect(&ImmediateRenderer::recreateSwapchain, this);
}

bool ImmediateRenderer::init() {
    LOG_INFO("Initializing ImmediateRenderer");

    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    m_descriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBuffer(0, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, true)
            .addCombinedImageSampler(1, vk::ShaderStageFlagBits::eFragment)
            .build("ImmediateRenderer-DescriptorSetLayout");

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->descriptorSet = DescriptorSet::create(m_descriptorSetLayout, descriptorPool, "ImmediateRenderer-DescriptorSet");
        m_resources[i]->updateDescriptors = true;
    }

    Engine::eventDispatcher()->connect(&ImmediateRenderer::recreateSwapchain, this);

    if (!createRenderPass()) {
        LOG_ERROR("Failed to create ImmediateRenderer RenderPass");
        return false;
    }
    return true;
}

void ImmediateRenderer::render(double dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("ImmediateRenderer::render");
    PROFILE_BEGIN_GPU_CMD("ImmediateRenderer::render", commandBuffer)

    if (m_resources->updateDescriptors && m_resources->descriptorSet != nullptr) {
        m_resources->updateDescriptors = false;
        DescriptorSetWriter(m_resources->descriptorSet)
            .writeImage(1, Engine::instance()->getDeferredRenderer()->getDepthSampler().get(), Engine::instance()->getDeferredRenderer()->getDepthImageView(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
            .write();
    }

    uploadBuffers();

    std::array<vk::DescriptorSet, 1> descriptorSets = { m_resources->descriptorSet->getDescriptorSet() };

    GraphicsPipeline* currentPipeline = nullptr;

    const RenderState* prevRenderState = nullptr;

    m_renderPass->begin(commandBuffer, m_resources->framebuffer, vk::SubpassContents::eInline);

    for (size_t index = 0; index < m_renderCommands.size(); ++index) {

        const auto& command = m_renderCommands[index];

        GraphicsPipeline* pipeline = getGraphicsPipeline(command);
        if (pipeline == nullptr)
            continue;

        if (currentPipeline != pipeline) {
            currentPipeline = pipeline;
            pipeline->bind(commandBuffer);
            prevRenderState = nullptr;
        }

        if (prevRenderState == nullptr || prevRenderState->depthTestEnabled != command.state.depthTestEnabled)
            pipeline->setDepthTestEnabled(commandBuffer, command.state.depthTestEnabled);
        if (prevRenderState == nullptr || prevRenderState->cullMode != command.state.cullMode)
            pipeline->setCullMode(commandBuffer, command.state.cullMode);
        if (prevRenderState == nullptr || prevRenderState->lineWidth != command.state.lineWidth)
            pipeline->setLineWidth(commandBuffer, command.state.lineWidth);

        vk::DeviceSize vertexBufferOffset = command.vertexOffset * sizeof(ColouredVertex);
        vk::DeviceSize indexBufferOffset = command.indexOffset * sizeof(uint32_t);

        vk::DeviceSize alignedUniformBufferSize = Engine::graphics()->getAlignedUniformBufferOffset(sizeof(UniformBufferData));
        std::array<uint32_t, 1> dynamicOffsets = { (uint32_t)(index * alignedUniformBufferSize) };

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);
        commandBuffer.bindVertexBuffers(0, 1, &m_resources->vertexBuffer->getBuffer(), &vertexBufferOffset);
        commandBuffer.bindIndexBuffer(m_resources->indexBuffer->getBuffer(), indexBufferOffset, vk::IndexType::eUint32);


        commandBuffer.drawIndexed(command.indexCount, 1, 0, 0, 0);
        prevRenderState = &command.state;
    }

    commandBuffer.endRenderPass();
    PROFILE_END_GPU_CMD("ImmediateRenderer::render", commandBuffer)

    m_renderCommands.clear();
    m_uniformBufferData.clear();
    m_vertexCount = 0;
    m_indexCount = 0;
    m_firstChangedVertex = (uint32_t)(-1);
    m_firstChangedIndex = (uint32_t)(-1);

    colour(glm::u8vec4(255, 255, 255, 255));
    normal(glm::vec3(0.0F, 0.0F, 0.0F));
    texture(glm::vec2(0.0F, 0.0F));
}

void ImmediateRenderer::begin(MeshPrimitiveType primitiveType) {

    if (m_currentCommand != nullptr) {
        LOG_ERROR("Cannot begin debug render group. Current group is not ended");
        assert(false);
        return;
    }

    m_currentCommand = &m_renderCommands.emplace_back();
    m_currentCommand->primitiveType = primitiveType;
    m_currentCommand->vertexOffset = m_vertexCount;
    m_currentCommand->indexOffset = m_indexCount;
    m_currentCommand->vertexCount = 0;
    m_currentCommand->indexCount = 0;
    m_currentCommand->state = m_renderState;
    UniformBufferData& uniformBufferData = m_uniformBufferData.emplace_back();
    uniformBufferData.modelViewMatrix = m_modelMatrixStack.top();
    uniformBufferData.projectionMatrix = m_projectionMatrixStack.top();
    uniformBufferData.resolution = Engine::graphics()->getResolution();
    uniformBufferData.depthTestEnabled = m_renderState.depthTestEnabled;
    uniformBufferData.useColour = m_renderState.useColour;
    uniformBufferData.frontfaceColour = m_renderState.frontfaceColour;
    uniformBufferData.backfaceColour = m_renderState.backfaceColour;

//    LOG_DEBUG("Begin modelViewMatrix:\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]",
//           m_currentCommand->modelViewMatrix[0][0], m_currentCommand->modelViewMatrix[1][0], m_currentCommand->modelViewMatrix[2][0], m_currentCommand->modelViewMatrix[3][0],
//           m_currentCommand->modelViewMatrix[0][1], m_currentCommand->modelViewMatrix[1][1], m_currentCommand->modelViewMatrix[2][1], m_currentCommand->modelViewMatrix[3][1],
//           m_currentCommand->modelViewMatrix[0][2], m_currentCommand->modelViewMatrix[1][2], m_currentCommand->modelViewMatrix[2][2], m_currentCommand->modelViewMatrix[3][2],
//           m_currentCommand->modelViewMatrix[0][3], m_currentCommand->modelViewMatrix[1][3], m_currentCommand->modelViewMatrix[2][3], m_currentCommand->modelViewMatrix[3][3]);
//    LOG_DEBUG("Begin projectionMatrix:\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]",
//           m_currentCommand->projectionMatrix[0][0], m_currentCommand->projectionMatrix[1][0], m_currentCommand->projectionMatrix[2][0], m_currentCommand->projectionMatrix[3][0],
//           m_currentCommand->projectionMatrix[0][1], m_currentCommand->projectionMatrix[1][1], m_currentCommand->projectionMatrix[2][1], m_currentCommand->projectionMatrix[3][1],
//           m_currentCommand->projectionMatrix[0][2], m_currentCommand->projectionMatrix[1][2], m_currentCommand->projectionMatrix[2][2], m_currentCommand->projectionMatrix[3][2],
//           m_currentCommand->projectionMatrix[0][3], m_currentCommand->projectionMatrix[1][3], m_currentCommand->projectionMatrix[2][3], m_currentCommand->projectionMatrix[3][3]);
}

void ImmediateRenderer::end() {

    if (m_currentCommand->primitiveType == PrimitiveType_LineLoop) {
        addIndex(0); // Loop back to the first vertex
    }

    m_currentCommand = nullptr;
}

void ImmediateRenderer::vertex(const glm::vec3& position) {
    ColouredVertex v(position, m_normal, m_texture, m_colour);

    uint32_t index = m_vertexCount - m_currentCommand->vertexOffset;

    addVertex(v);
    addIndex(index);
}

void ImmediateRenderer::vertex(float x, float y, float z) {
    this->vertex(glm::vec3(x, y, z));
}

void ImmediateRenderer::normal(const glm::vec3& normal) {
    m_normal = normal;
}

void ImmediateRenderer::normal(float x, float y, float z) {
    m_normal.x = x;
    m_normal.y = y;
    m_normal.z = z;
}

void ImmediateRenderer::texture(const glm::vec2& texture) {
    m_texture = texture;
}

void ImmediateRenderer::texture(float x, float y) {
    m_texture.x = x;
    m_texture.y = y;
}

void ImmediateRenderer::colour(const glm::u8vec4& colour) {
    m_colour = colour;
}

void ImmediateRenderer::colour(const glm::u8vec3& colour) {
    m_colour.r = colour.r;
    m_colour.g = colour.g;
    m_colour.b = colour.b;
    m_colour.a = 255;
}

void ImmediateRenderer::colour(const glm::uvec4& colour) {
    m_colour.r = glm::min(colour.r, 255u);
    m_colour.g = glm::min(colour.g, 255u);
    m_colour.b = glm::min(colour.b, 255u);
    m_colour.a = glm::min(colour.a, 255u);
}

void ImmediateRenderer::colour(const glm::uvec3& colour) {
    m_colour.r = glm::min(colour.r, 255u);
    m_colour.g = glm::min(colour.g, 255u);
    m_colour.b = glm::min(colour.b, 255u);
    m_colour.a = 255;
}

void ImmediateRenderer::colour(const glm::vec4& colour) {
    this->colour(colour.r, colour.g, colour.b, colour.a);
}

void ImmediateRenderer::colour(const glm::vec3& colour) {
    this->colour(colour.r, colour.g, colour.b);
}

void ImmediateRenderer::colour(float r, float g, float b, float a) {
    m_colour.r = (uint8_t)(glm::clamp(r, 0.0F, 1.0F) * 255.0F);
    m_colour.g = (uint8_t)(glm::clamp(g, 0.0F, 1.0F) * 255.0F);
    m_colour.b = (uint8_t)(glm::clamp(b, 0.0F, 1.0F) * 255.0F);
    m_colour.a = (uint8_t)(glm::clamp(a, 0.0F, 1.0F) * 255.0F);
}

void ImmediateRenderer::colour(float r, float g, float b) {
    m_colour.r = (uint8_t)(glm::clamp(r, 0.0F, 1.0F) * 255.0F);
    m_colour.g = (uint8_t)(glm::clamp(g, 0.0F, 1.0F) * 255.0F);
    m_colour.b = (uint8_t)(glm::clamp(b, 0.0F, 1.0F) * 255.0F);
    m_colour.a = (uint8_t)(255);
}

void ImmediateRenderer::pushMatrix(MatrixMode matrixMode) {
    auto& stack = matrixStack(matrixMode);

#if _DEBUG || IMMEDIATE_MODE_VALIDATION
    if (stack.size() > 256) {
        LOG_FATAL("ImmediateRenderer::pushMatrix - stack overflow");
        assert(false);
        return;
    }
#endif

    validateCompleteCommand();

    // Duplicate top of stack
    stack.push(stack.top());
}

void ImmediateRenderer::pushMatrix() {
    pushMatrix(m_matrixMode);
}

void ImmediateRenderer::popMatrix(MatrixMode matrixMode) {
    auto& stack = matrixStack(matrixMode);

#if _DEBUG || IMMEDIATE_MODE_VALIDATION
    if (stack.size() == 1) {
        LOG_FATAL("ImmediateRenderer::popMatrix - stack underflow");
        assert(false);
        return;
    }
#endif

    validateCompleteCommand();
    stack.pop();
}

void ImmediateRenderer::popMatrix() {
    popMatrix(m_matrixMode);
}

void ImmediateRenderer::translate(const glm::vec3& translation) {
    validateCompleteCommand();
    glm::mat4& currMat = currentMatrix();
    currMat = glm::translate(currMat, translation);
}
void ImmediateRenderer::translate(float x, float y, float z) {
    this->translate(glm::vec3(x, y, z));
}

void ImmediateRenderer::rotate(const glm::vec3& axis, float angle) {
    validateCompleteCommand();
    glm::mat4& currMat = currentMatrix();
    currMat = glm::rotate(currMat, angle, axis);
}
void ImmediateRenderer::rotate(float x, float y, float z, float angle) {
    this->rotate(glm::vec3(x, y, z), angle);
}

void ImmediateRenderer::scale(const glm::vec3& scale) {
    validateCompleteCommand();
    glm::mat4& currMat = currentMatrix();
    currMat = glm::scale(currMat, scale);
}

void ImmediateRenderer::scale(float x, float y, float z) {
    this->scale(glm::vec3(x, y, z));
}

void ImmediateRenderer::scale(float scale) {
    this->scale(glm::vec3(scale, scale, scale));
}

void ImmediateRenderer::loadIdentity() {
    validateCompleteCommand();
    currentMatrix() = glm::mat4(1.0F); // Identity matrix
}

void ImmediateRenderer::loadMatrix(const glm::mat4& matrix) {
    validateCompleteCommand();
    currentMatrix() = matrix;
}
void ImmediateRenderer::multMatrix(const glm::mat4& matrix) {
    validateCompleteCommand();
    glm::mat4& currMat = currentMatrix();
    currMat = currMat * matrix;
}

void ImmediateRenderer::matrixMode(MatrixMode matrixMode) {
#if _DEBUG || IMMEDIATE_MODE_VALIDATION
    validateCompleteCommand();
#endif
    m_matrixMode = matrixMode;
}

void ImmediateRenderer::setDepthTestEnabled(bool enabled) {
    validateCompleteCommand();
    m_renderState.depthTestEnabled = enabled;
}

void ImmediateRenderer::setCullMode(const vk::CullModeFlags& cullMode) {
    validateCompleteCommand();
    m_renderState.cullMode = cullMode;
}

void ImmediateRenderer::setColourMultiplierEnabled(bool enabled) {
    validateCompleteCommand();
    m_renderState.useColour = enabled;
}

void ImmediateRenderer::setFrontfaceColourMultiplier(const glm::vec4& colour) {
    validateCompleteCommand();
    m_renderState.frontfaceColour = colour;
}

void ImmediateRenderer::setBackfaceColourMultiplier(const glm::vec4& colour) {
    validateCompleteCommand();
    m_renderState.backfaceColour = colour;
}

void ImmediateRenderer::setBlendEnabled(bool enabled) {
    m_renderState.blendEnabled = enabled;
}

void ImmediateRenderer::setColourBlendMode(vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op) {
    m_renderState.colourBlendMode.src = src;
    m_renderState.colourBlendMode.dst = dst;
    m_renderState.colourBlendMode.op = op;
}

void ImmediateRenderer::setAlphaBlendMode(vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op) {
    m_renderState.alphaBlendMode.src = src;
    m_renderState.alphaBlendMode.dst = dst;
    m_renderState.alphaBlendMode.op = op;
}

void ImmediateRenderer::setLineWidth(float lineWidth) {
    m_renderState.lineWidth = lineWidth;
}

ImageView* ImmediateRenderer::getOutputFrameImageView() const {
    return m_resources->frameColourImageView;
}


void ImmediateRenderer::addVertex(const ColouredVertex& vertex) {

    constexpr float eps = std::numeric_limits<float>::epsilon();

    if (m_firstChangedVertex == (uint32_t)(-1))
        if (m_vertexCount >= m_vertices.size() || !vertex.equalsEpsilon(m_vertices[m_vertexCount], eps))
            m_firstChangedVertex = m_vertexCount;

    if (m_vertexCount >= m_vertices.size())
        m_vertices.emplace_back(vertex);
    else
        m_vertices[m_vertexCount] = vertex;

    ++m_currentCommand->vertexCount;
    ++m_vertexCount;
}

void ImmediateRenderer::addIndex(uint32_t index) {

    if (m_firstChangedIndex == (uint32_t)(-1))
        if (m_indexCount >= m_indices.size() || index != m_indices[m_indexCount])
            m_firstChangedIndex = m_indexCount;

    if (m_indexCount >= m_indices.size())
        m_indices.emplace_back(index);
    else
        m_indices[m_indexCount] = index;

    ++m_currentCommand->indexCount;
    ++m_indexCount;
}

glm::mat4& ImmediateRenderer::currentMatrix(MatrixMode matrixMode) {
    return matrixStack(matrixMode).top();
}

std::stack<glm::mat4>& ImmediateRenderer::matrixStack(MatrixMode matrixMode) {
    switch (matrixMode) {
        case MatrixMode_ModelView: return m_modelMatrixStack;
        case MatrixMode_Projection: return m_projectionMatrixStack;
        default: assert(false); break;
    }
    return m_modelMatrixStack; // We shouldn't reach here, but need to return something by default.
}

glm::mat4& ImmediateRenderer::currentMatrix() {
    return currentMatrix(m_matrixMode);
}

std::stack<glm::mat4>& ImmediateRenderer::matrixStack() {
    return matrixStack(m_matrixMode);
}

void ImmediateRenderer::uploadBuffers() {
    PROFILE_SCOPE("ImmediateRenderer::uploadBuffers");

//    if (m_vertices.capacity() > m_vertices.size() * 4)
//        m_vertices.shrink_to_fit();

    const size_t vertexBufferCapacity = m_vertices.capacity() * sizeof(ColouredVertex);
    if (m_resources->vertexBuffer == nullptr || m_resources->vertexBuffer->getSize() < vertexBufferCapacity) {
        PROFILE_REGION("Recreate vertex buffer");

        BufferConfiguration vertexBufferConfig{};
        vertexBufferConfig.device = Engine::graphics()->getDevice();
        vertexBufferConfig.size = glm::max(vertexBufferCapacity, sizeof(ColouredVertex) * 32);
        vertexBufferConfig.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
//        vertexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;// | vk::MemoryPropertyFlagBits::eHostVisible;
        vertexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        delete m_resources->vertexBuffer;
        m_resources->vertexBuffer = Buffer::create(vertexBufferConfig, "ImmediateRenderer-VertexBuffer");
    }

    const size_t indexBufferCapacity = m_indices.capacity() * sizeof(uint32_t);
    if (m_resources->indexBuffer == nullptr || m_resources->indexBuffer->getSize() < indexBufferCapacity) {
        PROFILE_REGION("Recreate index buffer");

        BufferConfiguration indexBufferConfig{};
        indexBufferConfig.device = Engine::graphics()->getDevice();
        indexBufferConfig.size = glm::max(indexBufferCapacity, sizeof(uint32_t) * 32);
        indexBufferConfig.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
//        indexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;// | vk::MemoryPropertyFlagBits::eHostVisible;
        indexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        delete m_resources->indexBuffer;
        m_resources->indexBuffer = Buffer::create(indexBufferConfig, "ImmediateRenderer-IndexBuffer");
    }

    vk::DeviceSize alignedUniformBufferSize = Engine::graphics()->getAlignedUniformBufferOffset(sizeof(UniformBufferData));
    const vk::DeviceSize uniformBufferCapacity = m_uniformBufferData.capacity() * alignedUniformBufferSize;
    if (m_resources->uniformBuffer == nullptr || m_resources->uniformBuffer->getSize() < uniformBufferCapacity) {
        PROFILE_REGION("Recreate uniform buffer");

        BufferConfiguration uniformBufferConfig{};
        uniformBufferConfig.device = Engine::graphics()->getDevice();
        uniformBufferConfig.size = glm::max(uniformBufferCapacity, alignedUniformBufferSize * 4);
        uniformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        uniformBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

        const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

        delete m_resources->uniformBuffer;
        m_resources->uniformBuffer = Buffer::create(uniformBufferConfig, "ImmediateRenderer-UniformBuffer");

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeBuffer(0, m_resources->uniformBuffer, 0, alignedUniformBufferSize).write();
    }

    PROFILE_REGION("Upload vertices");
    if (!m_vertices.empty()) {
        m_resources->vertexBuffer->upload(0, m_vertices.size() * sizeof(ColouredVertex), m_vertices.data());
//        if (m_firstChangedVertex != (uint32_t)(-1)) {
//            m_resources->vertexBuffer->upload(m_firstChangedVertex * sizeof(ColouredVertex),(m_vertices.size() - m_firstChangedVertex) * sizeof(ColouredVertex),&m_vertices[m_firstChangedVertex]);
//        }
    }

    PROFILE_REGION("Upload indices");
    if (!m_indices.empty()) {
        m_resources->indexBuffer->upload(0, m_indices.size() * sizeof(uint32_t), m_indices.data());
//        if (m_firstChangedIndex != (uint32_t)(-1)) {
//            m_resources->indexBuffer->upload(m_firstChangedIndex * sizeof(uint32_t),(m_indices.size() - m_firstChangedIndex) * sizeof(uint32_t),&m_indices[m_firstChangedIndex]);
//        }
    }

    PROFILE_REGION("Upload uniforms");
    if (!m_uniformBufferData.empty()) {
        m_resources->uniformBuffer->upload(0, m_uniformBufferData.size() * sizeof(UniformBufferData), m_uniformBufferData.data(), 0, alignedUniformBufferSize, sizeof(UniformBufferData));
    }
}

GraphicsPipeline* ImmediateRenderer::getGraphicsPipeline(const RenderCommand& renderCommand) {
    PROFILE_SCOPE("ImmediateRenderer::getGraphicsPipeline");

    vk::PrimitiveTopology primitiveTopology;

    switch (renderCommand.primitiveType) {
        case PrimitiveType_Triangle: primitiveTopology = vk::PrimitiveTopology::eTriangleList; break;
        case PrimitiveType_TriangleStrip: primitiveTopology = vk::PrimitiveTopology::eTriangleStrip; break;
        case PrimitiveType_Line: primitiveTopology = vk::PrimitiveTopology::eLineList; break;
        case PrimitiveType_LineStrip:
        case PrimitiveType_LineLoop: primitiveTopology = vk::PrimitiveTopology::eLineStrip; break;
        case PrimitiveType_Point: primitiveTopology = vk::PrimitiveTopology::ePointList; break;
        default: return nullptr;
    }

    size_t key = 0;
    std::hash_combine(key, (uint32_t)primitiveTopology);
    std::hash_combine(key, renderCommand.state.blendEnabled);
    std::hash_combine(key, renderCommand.state.blendEnabled ? renderCommand.state.colourBlendMode : BlendMode{});
    std::hash_combine(key, renderCommand.state.blendEnabled ? renderCommand.state.alphaBlendMode : BlendMode{});

    auto it = m_graphicsPipelines.find(key);

    if (it == m_graphicsPipelines.end() || it->second == nullptr) {
        PROFILE_REGION("Initialize pipeline");

        GraphicsPipelineConfiguration pipelineConfiguration{};
        pipelineConfiguration.device = Engine::graphics()->getDevice();
        pipelineConfiguration.renderPass = m_renderPass;
        pipelineConfiguration.setViewport(0, 0); // Default to full window resolution

        pipelineConfiguration.primitiveTopology = primitiveTopology;

        pipelineConfiguration.setDynamicState(vk::DynamicState::eDepthTestEnableEXT, true);
        pipelineConfiguration.setDynamicState(vk::DynamicState::eCullModeEXT, true);
        pipelineConfiguration.setDynamicState(vk::DynamicState::eLineWidth, true);

//        pipelineConfiguration.polygonMode = vk::PolygonMode::eLine;

        AttachmentBlendState attachmentBlendState;
        attachmentBlendState.blendEnable = renderCommand.state.blendEnabled;
        if (renderCommand.state.blendEnabled) {
            attachmentBlendState.setColourBlendMode(renderCommand.state.colourBlendMode);
            attachmentBlendState.setAlphaBlendMode(renderCommand.state.alphaBlendMode);
        }

        pipelineConfiguration.setAttachmentBlendState(0, attachmentBlendState);

        pipelineConfiguration.vertexShader = "shaders/debug/debug_lines.vert";
        pipelineConfiguration.fragmentShader = "shaders/debug/debug_lines.frag";

        pipelineConfiguration.vertexInputBindings.resize(1);
        pipelineConfiguration.vertexInputBindings[0].setBinding(0);
        pipelineConfiguration.vertexInputBindings[0].setStride(sizeof(ColouredVertex));
        pipelineConfiguration.vertexInputBindings[0].setInputRate(vk::VertexInputRate::eVertex);

        pipelineConfiguration.vertexInputAttributes.resize(4);
        pipelineConfiguration.vertexInputAttributes[0].setBinding(0);
        pipelineConfiguration.vertexInputAttributes[0].setLocation(0);
        pipelineConfiguration.vertexInputAttributes[0].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
        pipelineConfiguration.vertexInputAttributes[0].setOffset(offsetof(ColouredVertex, position));
        pipelineConfiguration.vertexInputAttributes[1].setBinding(0);
        pipelineConfiguration.vertexInputAttributes[1].setLocation(1);
        pipelineConfiguration.vertexInputAttributes[1].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
        pipelineConfiguration.vertexInputAttributes[1].setOffset(offsetof(ColouredVertex, normal));
        pipelineConfiguration.vertexInputAttributes[2].setBinding(0);
        pipelineConfiguration.vertexInputAttributes[2].setLocation(2);
        pipelineConfiguration.vertexInputAttributes[2].setFormat(vk::Format::eR32G32Sfloat); // vec2
        pipelineConfiguration.vertexInputAttributes[2].setOffset(offsetof(ColouredVertex, texture));
        pipelineConfiguration.vertexInputAttributes[3].setBinding(0);
        pipelineConfiguration.vertexInputAttributes[3].setLocation(3);
        pipelineConfiguration.vertexInputAttributes[3].setFormat(vk::Format::eR8G8B8A8Unorm); // u8vec4
        pipelineConfiguration.vertexInputAttributes[3].setOffset(offsetof(ColouredVertex, colour));

        pipelineConfiguration.descriptorSetLayouts.emplace_back(m_descriptorSetLayout->getDescriptorSetLayout());

        GraphicsPipeline* pipeline = GraphicsPipeline::create(pipelineConfiguration, "ImmediateRenderer-GraphicsPipeline");
        m_graphicsPipelines.insert(std::make_pair(key, pipeline));

        return pipeline;
    }

    return it->second;
}

bool ImmediateRenderer::createRenderPass() {
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    std::array<vk::AttachmentDescription, 2> attachments;

    attachments[0].setFormat(Engine::graphics()->getColourFormat());
    attachments[0].setSamples(samples);
    attachments[0].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[0].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[0].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[0].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[0].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[0].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    attachments[1].setFormat(Engine::graphics()->getDepthFormat());
    attachments[1].setSamples(samples);
    attachments[1].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[1].setStoreOp(vk::AttachmentStoreOp::eStore); // could be eDontCare ?
    attachments[1].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[1].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[1].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[1].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal); // eDepthStencilAttachmentOptimal if we don't need to sample the depth buffer


    std::array<SubpassConfiguration, 1> subpassConfigurations;
    subpassConfigurations[0].addColourAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);

    std::array<vk::SubpassDependency, 2> dependencies;
    dependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[0].setDstSubpass(0);
    dependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    dependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[0].setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
    dependencies[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[0].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    dependencies[1].setSrcSubpass(0);
    dependencies[1].setDstSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[1].setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[1].setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    dependencies[1].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[1].setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
    dependencies[1].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    RenderPassConfiguration renderPassConfig{};
    renderPassConfig.device = Engine::graphics()->getDevice();
    renderPassConfig.setAttachments(attachments);
    renderPassConfig.setSubpasses(subpassConfigurations);
    renderPassConfig.setSubpassDependencies(dependencies);
//    renderPassConfig.setClearColour(0, glm::vec4(0.0F, 0.0F, 0.0F, 1.0F));

    m_renderPass = SharedResource<RenderPass>(RenderPass::create(renderPassConfig, "PostProcessRenderer-BloomBlurRenderPass"));
    return (bool)m_renderPass;
}

void ImmediateRenderer::recreateSwapchain(RecreateSwapchainEvent* event) {
    for (auto& [key, graphicsPipeline] : m_graphicsPipelines)
        delete graphicsPipeline; // Delete the existing pipelines.
    m_graphicsPipelines.clear();

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->updateDescriptors = true;
        delete m_resources[i]->framebuffer;
        delete m_resources[i]->frameColourImageView;
        delete m_resources[i]->frameColourImage;
        delete m_resources[i]->frameDepthImageView;
        delete m_resources[i]->frameDepthImage;

        vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

        Image2DConfiguration imageConfig{};
        imageConfig.device = Engine::graphics()->getDevice();
        imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        imageConfig.sampleCount = sampleCount;
        imageConfig.setSize(Engine::graphics()->getResolution());

        ImageViewConfiguration imageViewConfig{};
        imageViewConfig.device = Engine::graphics()->getDevice();

        // Colour Image
        imageConfig.format = Engine::graphics()->getColourFormat();
        imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
        m_resources[i]->frameColourImage = Image2D::create(imageConfig, "ImmediateRenderer-FrameColourImage");
        imageViewConfig.format = imageConfig.format;
        imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageViewConfig.setImage(m_resources[i]->frameColourImage);
        m_resources[i]->frameColourImageView = ImageView::create(imageViewConfig, "ImmediateRenderer-FrameColourImageView");

        // Depth Image
        imageConfig.format = Engine::graphics()->getDepthFormat();
        imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment;
        m_resources[i]->frameDepthImage = Image2D::create(imageConfig, "ImmediateRenderer-FrameDepthImage");
        imageViewConfig.format = imageConfig.format;
        imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eDepth;
        imageViewConfig.setImage(m_resources[i]->frameDepthImage);
        m_resources[i]->frameDepthImageView = ImageView::create(imageViewConfig, "ImmediateRenderer-FrameDepthImageView");

        // Framebuffer
        FramebufferConfiguration framebufferConfig{};
        framebufferConfig.device = Engine::graphics()->getDevice();
        framebufferConfig.setSize(Engine::graphics()->getResolution());
        framebufferConfig.setRenderPass(m_renderPass.get());
        framebufferConfig.addAttachment(m_resources[i]->frameColourImageView);
        framebufferConfig.addAttachment(m_resources[i]->frameDepthImageView);

        m_resources[i]->framebuffer = Framebuffer::create(framebufferConfig, "ImmediateRenderer-Framebuffer");
        assert(m_resources[i]->framebuffer != nullptr);
    }
}

void ImmediateRenderer::validateCompleteCommand() const {
#if _DEBUG || IMMEDIATE_MODE_VALIDATION
    if (m_currentCommand != nullptr) {
        LOG_FATAL("ImmediateRenderer error: Incomplete command");
        assert(false);
    }
#endif
}

