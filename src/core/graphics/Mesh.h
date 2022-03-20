
#ifndef WORLDENGINE_MESH_H
#define WORLDENGINE_MESH_H

#include "../core.h"

#include "GraphicsManager.h"
#include "Buffer.h"
#include "../engine/geometry/MeshData.h"


struct MeshConfiguration {
    std::weak_ptr<vkr::Device> device;
    const MeshData::Vertex* vertices;
    size_t vertexCount;
    const MeshData::Index* indices;
    size_t indexCount;

    void setVertices(const std::vector<MeshData::Vertex>& verticesArray);

    void setIndices(const std::vector<MeshData::Index>& indicesArray);

    void setIndices(const std::vector<MeshData::Triangle>& triangleArray);

    void setMeshData(MeshData* meshData);
};

class Mesh {
    NO_COPY(Mesh);
private:
    Mesh(std::weak_ptr<vkr::Device> device);

public:
    ~Mesh();

    static Mesh* create(const MeshConfiguration& meshConfiguration);

    bool uploadVertices(const MeshData::Vertex* vertices, size_t vertexCount);

    bool uploadVertices(const std::vector<MeshData::Vertex>& vertices);

    bool uploadIndices(const MeshData::Index* indices, size_t indexCount);

    bool uploadIndices(const std::vector<MeshData::Index>& indices);

    void draw(const vk::CommandBuffer& commandBuffer, uint32_t instanceCount, uint32_t firstInstance);

    uint32_t getVertexCount() const;

    uint32_t getIndexCount() const;

    const GraphicsResource& getResourceId() const;

    template<class T = MeshData::Vertex>
    static std::vector<vk::VertexInputBindingDescription> getVertexBindingDescriptions();

    template<class T = MeshData::Vertex>
    static std::vector<vk::VertexInputAttributeDescription> getVertexAttributeDescriptions();

private:
    std::shared_ptr<vkr::Device> m_device;
    Buffer* m_vertexBuffer;
    Buffer* m_indexBuffer;
    GraphicsResource m_resourceId;
};

template<>
inline std::vector<vk::VertexInputBindingDescription> Mesh::getVertexBindingDescriptions<MeshData::Vertex>() {
    std::vector<vk::VertexInputBindingDescription> inputBindingDescriptions;
    inputBindingDescriptions.resize(1);
    inputBindingDescriptions[0].setBinding(0);
    inputBindingDescriptions[0].setStride(sizeof(MeshData::Vertex));
    inputBindingDescriptions[0].setInputRate(vk::VertexInputRate::eVertex);
    return inputBindingDescriptions;
}

template<>
inline std::vector<vk::VertexInputAttributeDescription> Mesh::getVertexAttributeDescriptions<MeshData::Vertex>() {
    std::vector<vk::VertexInputAttributeDescription> attribDescriptions;
    attribDescriptions.resize(3);

    attribDescriptions[0].setBinding(0);
    attribDescriptions[0].setLocation(0);
    attribDescriptions[0].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
    attribDescriptions[0].setOffset(offsetof(MeshData::Vertex, position));

    attribDescriptions[1].setBinding(0);
    attribDescriptions[1].setLocation(1);
    attribDescriptions[1].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
    attribDescriptions[1].setOffset(offsetof(MeshData::Vertex, normal));

    attribDescriptions[2].setBinding(0);
    attribDescriptions[2].setLocation(2);
    attribDescriptions[2].setFormat(vk::Format::eR32G32Sfloat); // vec2
    attribDescriptions[2].setOffset(offsetof(MeshData::Vertex, texture));
    return attribDescriptions;
}

#endif //WORLDENGINE_MESH_H
