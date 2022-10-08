#include "core/graphics/Mesh.h"
#include "core/graphics/DeviceMemory.h"
#include "core/application/Engine.h"
#include <chrono>







void MeshConfiguration::setPrimitiveType(const MeshPrimitiveType& primitiveType) {
    this->primitiveType = primitiveType;
}



Mesh::Mesh(std::weak_ptr<vkr::Device> device):
        m_device(device),
        m_resourceId(GraphicsManager::nextResourceId()),
        m_vertexBuffer(nullptr),
        m_indexBuffer(nullptr),
        m_primitiveType(PrimitiveType_Triangle) {
}

Mesh::~Mesh() {
    delete m_vertexBuffer;
    delete m_indexBuffer;
}

Mesh* Mesh::create(const MeshConfiguration& meshConfiguration) {

    Mesh* mesh = new Mesh(meshConfiguration.device);

    if (meshConfiguration.vertexCount > 0) {
        if (!mesh->uploadVertices(meshConfiguration.vertices, meshConfiguration.vertexSize, meshConfiguration.vertexCount)) {
            printf("Unable to create mesh, failed to upload vertices\n");
            delete mesh;
            return nullptr;
        }
    }

    if (meshConfiguration.indexCount > 0) {
        if (!mesh->uploadIndices(meshConfiguration.indices, meshConfiguration.indexSize, meshConfiguration.indexCount)) {
            printf("Unable to create mesh, failed to upload indices\n");
            delete mesh;
            return nullptr;
        }
    }

    return mesh;
}

bool Mesh::uploadVertices(const void* vertices, const vk::DeviceSize& vertexSize, const size_t& vertexCount) {
    PROFILE_SCOPE("Mesh::uploadVertices")
    delete m_vertexBuffer;

    if (vertexCount <= 0) {
        // Valid to pass no vertices, we just deleted the buffer
        return true;
    }

    assert(vertices != nullptr);
    assert(vertexSize != 0);

    BufferConfiguration vertexBufferConfig;
    vertexBufferConfig.device = m_device;
    vertexBufferConfig.size = (vk::DeviceSize)(vertexCount) * vertexSize;
    vertexBufferConfig.data = (void*)vertices;
    vertexBufferConfig.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    vertexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    m_vertexBuffer = Buffer::create(vertexBufferConfig);
    m_vertexSize = vertexSize;

    if (m_vertexBuffer == nullptr) {
        printf("Failed to create vertex buffer\n");
        return false;
    }

    return true;
}

bool Mesh::uploadIndices(const void* indices, const vk::DeviceSize& indexSize, const size_t& indexCount) {
    PROFILE_SCOPE("Mesh::uploadIndices")
    delete m_indexBuffer;

    if (indexCount <= 0) {
        // Valid to pass no vertices, we just deleted the buffer
        return true;
    }

    assert(indices != nullptr);
    assert(indexSize != 0);

    BufferConfiguration indexBufferConfig;
    indexBufferConfig.device = m_device;
    indexBufferConfig.size = (vk::DeviceSize)indexCount * indexSize;
    indexBufferConfig.data = (void*)indices;
    indexBufferConfig.usage = vk::BufferUsageFlagBits::eIndexBuffer;
    indexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    m_indexBuffer = Buffer::create(indexBufferConfig);
    m_indexSize = indexSize;

    if (m_indexBuffer == nullptr) {
        printf("Failed to create vertex buffer\n");
        return false;
    }

    return true;
}

void Mesh::setPrimitiveType(const MeshPrimitiveType& primitiveType) {
    m_primitiveType = primitiveType;
}

void Mesh::draw(const vk::CommandBuffer& commandBuffer, const uint32_t& instanceCount, const uint32_t& firstInstance) {
    PROFILE_SCOPE("Mesh::draw")

    const vk::Buffer& vertexBuffer = m_vertexBuffer->getBuffer();
    const vk::DeviceSize offset = 0;

    commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer, &offset);
    commandBuffer.bindIndexBuffer(m_indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

    auto start = std::chrono::high_resolution_clock::now();

    commandBuffer.drawIndexed(getIndexCount(), instanceCount, 0, 0, firstInstance);

    auto end = std::chrono::high_resolution_clock::now();

    uint64_t elapsedNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // if debug
    size_t numIndices = (size_t)(m_indexBuffer->getSize() / m_indexSize);
    size_t numVertices = (size_t)(m_vertexBuffer->getSize() / m_vertexSize);
    Engine::graphics()->debugInfo().renderedPolygons += MeshUtils::getPolygonCount(numIndices, m_primitiveType);
    Engine::graphics()->debugInfo().renderedIndices += numIndices * instanceCount;
    Engine::graphics()->debugInfo().renderedVertices += numVertices * instanceCount;
    Engine::graphics()->debugInfo().drawCalls++;
    Engine::graphics()->debugInfo().drawInstances += instanceCount;
    Engine::graphics()->debugInfo().elapsedDrawNanosCPU += elapsedNanos;
}

uint32_t Mesh::getVertexCount() const {
    return (uint32_t)(m_vertexBuffer->getSize() / m_vertexSize);
}

uint32_t Mesh::getIndexCount() const {
    return (uint32_t)(m_indexBuffer->getSize() / m_indexSize);
}

const MeshPrimitiveType& Mesh::getPrimitiveType() const {
    return m_primitiveType;
}

const GraphicsResource& Mesh::getResourceId() const {
    return m_resourceId;
}
