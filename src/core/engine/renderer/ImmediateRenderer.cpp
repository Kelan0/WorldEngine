
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/application/Engine.h"
#include "core/graphics/DescriptorSet.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"





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

ImmediateRenderer::ColouredVertex::ColouredVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, const uint8_t& r, const uint8_t& g, const uint8_t& b, const uint8_t& a):
        ColouredVertex(glm::vec3(px, py, pz), glm::vec3(nx, ny, nz), glm::vec2(tx, ty), glm::u8vec4(r, g, b, a)) {
}

ImmediateRenderer::ColouredVertex::ColouredVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, const float& r, const float& g, const float& b, const float& a):
        ColouredVertex(glm::vec3(px, py, pz), glm::vec3(nx, ny, nz), glm::vec2(tx, ty), glm::vec4(r, g, b, a)) {
}


bool ImmediateRenderer::ColouredVertex::equalsEpsilon(const ColouredVertex& vertex, const float& epsilon) const {
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
        m_vertexBuffer(nullptr),
        m_indexBuffer(nullptr),
        m_vertexCount(0),
        m_indexCount(0),
        m_firstChangedVertex(0),
        m_firstChangedIndex(0) {
    m_modelMatrixStack.push(glm::mat4(1.0F));
    m_projectionMatrixStack.push(glm::mat4(1.0F));

    colour(glm::u8vec4(255, 255, 255, 255));
    normal(glm::vec3(0.0F, 0.0F, 0.0F));
    texture(glm::vec2(0.0F, 0.0F));
}

ImmediateRenderer::~ImmediateRenderer() {
    delete m_vertexBuffer;
    delete m_indexBuffer;
    for (auto it = m_graphicsPipelines.begin(); it != m_graphicsPipelines.end(); ++it)
        delete it->second;
}

bool ImmediateRenderer::init() {

    std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    m_descriptorSetLayout = builder.addUniformBuffer(0, vk::ShaderStageFlagBits::eVertex, true)
            .build("ImmediateRenderer-DescriptorSetLayout");

    Engine::eventDispatcher()->connect(&ImmediateRenderer::recreateSwapchain, this);

    return true;
}

void ImmediateRenderer::render(const double& dt) {
    PROFILE_SCOPE("ImmediateRenderer::render");

    uploadBuffers();

    const vk::CommandBuffer& commandBuffer = Engine::graphics()->getCurrentCommandBuffer();

    const vk::DescriptorSet& descriptorSet = m_descriptorSet->getDescriptorSet();

    GraphicsPipeline* currentPipeline = nullptr;

    const RenderState* prevRenderState = nullptr;

    for (size_t index = 0; index < m_renderCommands.size(); ++index) {
        PROFILE_REGION("Execute render command");

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

        uint32_t dynamicOffset = (uint32_t)(index * sizeof(UniformBufferData));

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getPipelineLayout(), 0, 1, &descriptorSet, 1, &dynamicOffset);
        commandBuffer.bindVertexBuffers(0, 1, &m_vertexBuffer->getBuffer(), &vertexBufferOffset);
        commandBuffer.bindIndexBuffer(m_indexBuffer->getBuffer(), indexBufferOffset, vk::IndexType::eUint32);


        commandBuffer.drawIndexed(command.indexCount, 1, 0, 0, 0);
        prevRenderState = &command.state;
    }

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

void ImmediateRenderer::begin(const MeshPrimitiveType& primitiveType) {
    PROFILE_SCOPE("ImmediateRenderer::begin");

    if (m_currentCommand != nullptr) {
        printf("Cannot begin debug render group. Current group is not ended\n");
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

//    printf("Begin modelViewMatrix:\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n",
//           m_currentCommand->modelViewMatrix[0][0], m_currentCommand->modelViewMatrix[1][0], m_currentCommand->modelViewMatrix[2][0], m_currentCommand->modelViewMatrix[3][0],
//           m_currentCommand->modelViewMatrix[0][1], m_currentCommand->modelViewMatrix[1][1], m_currentCommand->modelViewMatrix[2][1], m_currentCommand->modelViewMatrix[3][1],
//           m_currentCommand->modelViewMatrix[0][2], m_currentCommand->modelViewMatrix[1][2], m_currentCommand->modelViewMatrix[2][2], m_currentCommand->modelViewMatrix[3][2],
//           m_currentCommand->modelViewMatrix[0][3], m_currentCommand->modelViewMatrix[1][3], m_currentCommand->modelViewMatrix[2][3], m_currentCommand->modelViewMatrix[3][3]);
//    printf("Begin projectionMatrix:\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n[%.2f %.2f %.2f %.2f]\n",
//           m_currentCommand->projectionMatrix[0][0], m_currentCommand->projectionMatrix[1][0], m_currentCommand->projectionMatrix[2][0], m_currentCommand->projectionMatrix[3][0],
//           m_currentCommand->projectionMatrix[0][1], m_currentCommand->projectionMatrix[1][1], m_currentCommand->projectionMatrix[2][1], m_currentCommand->projectionMatrix[3][1],
//           m_currentCommand->projectionMatrix[0][2], m_currentCommand->projectionMatrix[1][2], m_currentCommand->projectionMatrix[2][2], m_currentCommand->projectionMatrix[3][2],
//           m_currentCommand->projectionMatrix[0][3], m_currentCommand->projectionMatrix[1][3], m_currentCommand->projectionMatrix[2][3], m_currentCommand->projectionMatrix[3][3]);
}

void ImmediateRenderer::end() {
    PROFILE_SCOPE("ImmediateRenderer::end");

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

void ImmediateRenderer::vertex(const float& x, const float& y, const float& z) {
    this->vertex(glm::vec3(x, y, z));
}

void ImmediateRenderer::normal(const glm::vec3& normal) {
    m_normal = normal;
}

void ImmediateRenderer::normal(const float& x, const float& y, const float& z) {
    m_normal.x = x;
    m_normal.y = y;
    m_normal.z = z;
}

void ImmediateRenderer::texture(const glm::vec2& texture) {
    m_texture = texture;
}

void ImmediateRenderer::texture(const float& x, const float& y) {
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

void ImmediateRenderer::colour(const float& r, const float& g, const float& b, const float& a) {
    m_colour.r = (uint8_t)(glm::clamp(r, 0.0F, 1.0F) * 255.0F);
    m_colour.g = (uint8_t)(glm::clamp(g, 0.0F, 1.0F) * 255.0F);
    m_colour.b = (uint8_t)(glm::clamp(b, 0.0F, 1.0F) * 255.0F);
    m_colour.a = (uint8_t)(glm::clamp(a, 0.0F, 1.0F) * 255.0F);
}

void ImmediateRenderer::colour(const float& r, const float& g, const float& b) {
    m_colour.r = (uint8_t)(glm::clamp(r, 0.0F, 1.0F) * 255.0F);
    m_colour.g = (uint8_t)(glm::clamp(g, 0.0F, 1.0F) * 255.0F);
    m_colour.b = (uint8_t)(glm::clamp(b, 0.0F, 1.0F) * 255.0F);
    m_colour.a = (uint8_t)(255);
}

void ImmediateRenderer::pushMatrix(const MatrixMode& matrixMode) {
    PROFILE_SCOPE("ImmediateRenderer::pushMatrix");
    auto& stack = matrixStack(matrixMode);

#if _DEBUG || IMMEDIATE_MODE_VALIDATION
    if (stack.size() > 256) {
        printf("ImmediateRenderer::pushMatrix - stack overflow\n");
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

void ImmediateRenderer::popMatrix(const MatrixMode& matrixMode) {
    PROFILE_SCOPE("ImmediateRenderer::popMatrix");
    auto& stack = matrixStack(matrixMode);

#if _DEBUG || IMMEDIATE_MODE_VALIDATION
    if (stack.size() == 1) {
        printf("ImmediateRenderer::popMatrix - stack underflow\n");
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
void ImmediateRenderer::translate(const float& x, const float& y, const float& z) {
    this->translate(glm::vec3(x, y, z));
}

void ImmediateRenderer::rotate(const glm::vec3& axis, const float& angle) {
    validateCompleteCommand();
    glm::mat4& currMat = currentMatrix();
    currMat = glm::rotate(currMat, angle, axis);
}
void ImmediateRenderer::rotate(const float& x, const float& y, const float& z, const float& angle) {
    this->rotate(glm::vec3(x, y, z), angle);
}

void ImmediateRenderer::scale(const glm::vec3& scale) {
    validateCompleteCommand();
    glm::mat4& currMat = currentMatrix();
    currMat = glm::scale(currMat, scale);
}

void ImmediateRenderer::scale(const float& x, const float& y, const float& z) {
    this->scale(glm::vec3(x, y, z));
}

void ImmediateRenderer::scale(const float& scale) {
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

void ImmediateRenderer::matrixMode(const MatrixMode& matrixMode) {
#if _DEBUG || IMMEDIATE_MODE_VALIDATION
    validateCompleteCommand();
#endif
    m_matrixMode = matrixMode;
}

void ImmediateRenderer::setDepthTestEnabled(const bool& enabled) {
    validateCompleteCommand();
    m_renderState.depthTestEnabled = enabled;
}

void ImmediateRenderer::setCullMode(const vk::CullModeFlags& cullMode) {
    validateCompleteCommand();
    m_renderState.cullMode = cullMode;
}

void ImmediateRenderer::setBlendEnabled(const bool& enabled) {
    m_renderState.blendEnabled = enabled;
}

void ImmediateRenderer::setColourBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op) {
    m_renderState.colourBlendMode.src = src;
    m_renderState.colourBlendMode.dst = dst;
    m_renderState.colourBlendMode.op = op;
}

void ImmediateRenderer::setAlphaBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op) {
    m_renderState.alphaBlendMode.src = src;
    m_renderState.alphaBlendMode.dst = dst;
    m_renderState.alphaBlendMode.op = op;
}

void ImmediateRenderer::setLineWidth(const float& lineWidth) {
    m_renderState.lineWidth = lineWidth;
}


void ImmediateRenderer::addVertex(const ColouredVertex& vertex) {
    PROFILE_SCOPE("ImmediateRenderer::addVertex");

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

void ImmediateRenderer::addIndex(const uint32_t& index) {
    PROFILE_SCOPE("ImmediateRenderer::addIndex");

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

glm::mat4& ImmediateRenderer::currentMatrix(const MatrixMode& matrixMode) {
    return matrixStack(matrixMode).top();
}

std::stack<glm::mat4>& ImmediateRenderer::matrixStack(const MatrixMode& matrixMode) {
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

    const size_t vertexBufferCapacity = m_vertices.capacity() * sizeof(ColouredVertex);
    if (m_vertexBuffer == nullptr || m_vertexBuffer->getSize() < vertexBufferCapacity) {
        PROFILE_REGION("Recreate vertex buffer");

        BufferConfiguration vertexBufferConfig{};
        vertexBufferConfig.device = Engine::graphics()->getDevice();
        vertexBufferConfig.size = glm::max(vertexBufferCapacity, sizeof(ColouredVertex) * 32);
        vertexBufferConfig.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
//        vertexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;// | vk::MemoryPropertyFlagBits::eHostVisible;
        vertexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        delete m_vertexBuffer;
        m_vertexBuffer = Buffer::create(vertexBufferConfig, "ImmediateRenderer-VertexBuffer");
    }

    const size_t indexBufferCapacity = m_indices.capacity() * sizeof(uint32_t);
    if (m_indexBuffer == nullptr || m_indexBuffer->getSize() < indexBufferCapacity) {
        PROFILE_REGION("Recreate index buffer");

        BufferConfiguration indexBufferConfig{};
        indexBufferConfig.device = Engine::graphics()->getDevice();
        indexBufferConfig.size = glm::max(indexBufferCapacity, sizeof(uint32_t) * 32);
        indexBufferConfig.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
//        indexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;// | vk::MemoryPropertyFlagBits::eHostVisible;
        indexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        delete m_indexBuffer;
        m_indexBuffer = Buffer::create(indexBufferConfig, "ImmediateRenderer-IndexBuffer");
    }

    const size_t uniformBufferCapacity = m_uniformBufferData.capacity() * sizeof(UniformBufferData);
    if (m_uniformBuffer == nullptr || m_uniformBuffer->getSize() < uniformBufferCapacity) {
        PROFILE_REGION("Recreate uniform buffer");

        BufferConfiguration uniformBufferConfig{};
        uniformBufferConfig.device = Engine::graphics()->getDevice();
        uniformBufferConfig.size = glm::max(uniformBufferCapacity, sizeof(UniformBufferData) * 4);
        uniformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        uniformBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

        std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();

        m_uniformBuffer = Buffer::create(uniformBufferConfig, "ImmediateRenderer-UniformBuffer");
        m_descriptorSet = DescriptorSet::create(m_descriptorSetLayout, descriptorPool, "ImmediateRenderer-DescriptorSet");

        DescriptorSetWriter(m_descriptorSet.get())
                .writeBuffer(0, m_uniformBuffer.get(), 0, sizeof(UniformBufferData)).write();
    }

    PROFILE_REGION("Upload vertices");
    if (!m_vertices.empty()) {
//        m_vertexBuffer->upload(0, m_vertices.size() * sizeof(ColouredVertex), m_vertices.data());
        if (m_firstChangedVertex != (uint32_t)(-1)) {
            m_vertexBuffer->upload(m_firstChangedVertex * sizeof(ColouredVertex),(m_vertices.size() - m_firstChangedVertex) * sizeof(ColouredVertex),&m_vertices[m_firstChangedVertex]);
        }
    }

    PROFILE_REGION("Upload indices");
    if (!m_indices.empty()) {
//        m_indexBuffer->upload(0, m_indices.size() * sizeof(uint32_t), m_indices.data());
        if (m_firstChangedIndex != (uint32_t)(-1)) {
            m_indexBuffer->upload(m_firstChangedIndex * sizeof(uint32_t),(m_indices.size() - m_firstChangedIndex) * sizeof(uint32_t),&m_indices[m_firstChangedIndex]);
        }
    }

    PROFILE_REGION("Upload uniforms");
    if (!m_uniformBufferData.empty()) {
        m_uniformBuffer->upload(0, m_uniformBufferData.size() * sizeof(UniformBufferData), m_uniformBufferData.data());
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
        pipelineConfiguration.renderPass = Engine::graphics()->renderPass();
        pipelineConfiguration.setViewport(0, 0); // Default to full window resolution

        pipelineConfiguration.primitiveTopology = primitiveTopology;

        pipelineConfiguration.setDynamicState(vk::DynamicState::eDepthTestEnableEXT, true);
        pipelineConfiguration.setDynamicState(vk::DynamicState::eCullModeEXT, true);
        pipelineConfiguration.setDynamicState(vk::DynamicState::eLineWidth, true);

        AttachmentBlendState attachmentBlendState;
        attachmentBlendState.blendEnable = renderCommand.state.blendEnabled;
        if (renderCommand.state.blendEnabled) {
            attachmentBlendState.setColourBlendMode(renderCommand.state.colourBlendMode);
            attachmentBlendState.setAlphaBlendMode(renderCommand.state.alphaBlendMode);
        }

        pipelineConfiguration.setAttachmentBlendState(0, attachmentBlendState);

        pipelineConfiguration.vertexShader = "res/shaders/debug/debug_lines.vert";
        pipelineConfiguration.fragmentShader = "res/shaders/debug/debug_lines.frag";

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

void ImmediateRenderer::recreateSwapchain(const RecreateSwapchainEvent& event) {
    for (auto it = m_graphicsPipelines.begin(); it != m_graphicsPipelines.end(); ++it)
        delete it->second; // Delete the existing pipelines.

    m_graphicsPipelines.clear();
}

void ImmediateRenderer::validateCompleteCommand() const {
#if _DEBUG || IMMEDIATE_MODE_VALIDATION
    if (m_currentCommand != nullptr) {
        printf("ImmediateRenderer error: Incomplete command\n");
        assert(false);
    }
#endif
}

