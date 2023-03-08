#include "core/graphics/Mesh.h"
#include "core/graphics/DeviceMemory.h"
#include "core/application/Engine.h"
#include "core/util/Profiler.h"
#include <chrono>



Mesh::Mesh(const WeakResource<vkr::Device>& device, const std::string& name):
        GraphicsResource(ResourceType_Mesh, device, name),
        m_vertexSize(0),
        m_indexSize(0),
        m_vertexBuffer(nullptr),
        m_indexBuffer(nullptr) {
}

Mesh::~Mesh() {
    delete m_vertexBuffer;
    delete m_indexBuffer;
}

Mesh* Mesh::create(const MeshConfiguration& meshConfiguration, const std::string& name) {

    Mesh* mesh = new Mesh(meshConfiguration.device, name);

    if (meshConfiguration.vertexCount > 0) {
        if (!mesh->uploadVertices(meshConfiguration.vertices, meshConfiguration.vertexSize, meshConfiguration.vertexCount)) {
            printf("Unable to create mesh \"%s\": failed to upload vertices\n", name.c_str());
            delete mesh;
            return nullptr;
        }
    }

    if (meshConfiguration.indexCount > 0) {
        if (!mesh->uploadIndices(meshConfiguration.indices, meshConfiguration.indexSize, meshConfiguration.indexCount)) {
            printf("Unable to create mesh \"%s\": failed to upload indices\n", name.c_str());
            delete mesh;
            return nullptr;
        }
    }

    return mesh;
}

bool Mesh::uploadVertices(const void* vertices, vk::DeviceSize vertexSize, size_t vertexCount) {
    PROFILE_SCOPE("Mesh::uploadVertices")
    delete m_vertexBuffer;
    m_vertexBuffer = nullptr;

    if (vertexCount <= 0) {
        // Valid to pass no vertices, we just deleted the buffer
        return true;
    }

    assert(vertices != nullptr);
    assert(vertexSize != 0);

    BufferConfiguration vertexBufferConfig{};
    vertexBufferConfig.device = m_device;
    vertexBufferConfig.size = (vk::DeviceSize)(vertexCount) * vertexSize;
    vertexBufferConfig.data = (void*)vertices;
    vertexBufferConfig.usage = vk::BufferUsageFlagBits::eVertexBuffer;
    vertexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    m_vertexBuffer = Buffer::create(vertexBufferConfig, m_name + "MeshVertexBuffer");
    m_vertexSize = vertexSize;

    if (m_vertexBuffer == nullptr) {
        printf("Failed to upload vertex buffer data for mesh \"%s\"\n", m_name.c_str());
        return false;
    }

    return true;
}

bool Mesh::uploadIndices(const void* indices, vk::DeviceSize indexSize, size_t indexCount) {
    PROFILE_SCOPE("Mesh::uploadIndices")
    delete m_indexBuffer;
    m_indexBuffer = nullptr;
    m_indexSize = 0;

    if (indexCount <= 0) {
        // Valid to pass no vertices, we just deleted the buffer
        return true;
    }

    assert(indices != nullptr);
    assert(indexSize != 0);

    BufferConfiguration indexBufferConfig{};
    indexBufferConfig.device = m_device;
    indexBufferConfig.size = (vk::DeviceSize)indexCount * indexSize;
    indexBufferConfig.data = (void*)indices;
    indexBufferConfig.usage = vk::BufferUsageFlagBits::eIndexBuffer;
    indexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    m_indexBuffer = Buffer::create(indexBufferConfig, m_name + "-MeshIndexBuffer");

    if (m_indexBuffer == nullptr) {
        printf("Failed to upload index buffer data for mesh \"%s\"\n", m_name.c_str());
        return false;
    }

    m_indexSize = indexSize;
    return true;
}

void Mesh::draw(const vk::CommandBuffer& commandBuffer, uint32_t instanceCount, uint32_t firstInstance) {
    PROFILE_SCOPE("Mesh::draw")

#if TRACK_DRAW_DEBUG_INFO
    auto start = std::chrono::high_resolution_clock::now();
#endif

    assert(m_vertexBuffer != nullptr);

    const vk::Buffer& vertexBuffer = m_vertexBuffer->getBuffer();
    const vk::DeviceSize offset = 0;

    commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer, &offset);
    if (m_indexBuffer != nullptr) {
        commandBuffer.bindIndexBuffer(m_indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexed(getIndexCount(), instanceCount, 0, 0, firstInstance);
    } else {
        commandBuffer.draw(getVertexCount(), instanceCount, 0, firstInstance);
    }

#if TRACK_DRAW_DEBUG_INFO
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
#endif
}

void Mesh::reset() {
    delete m_vertexBuffer;
    m_vertexBuffer = nullptr;
    m_vertexSize = 0;
    delete m_indexBuffer;
    m_indexBuffer = nullptr;
    m_indexSize = 0;
}

uint32_t Mesh::getVertexCount() const {
    return (uint32_t)(m_vertexBuffer->getSize() / m_vertexSize);
}

uint32_t Mesh::getIndexCount() const {
    return (uint32_t)(m_indexBuffer->getSize() / m_indexSize);
}

bool Mesh::hasIndices() const {
    return m_indexBuffer != nullptr;
}
