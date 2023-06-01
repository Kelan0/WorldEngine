
#ifndef WORLDENGINE_MESH_H
#define WORLDENGINE_MESH_H

#include "core/core.h"

#include "core/graphics/GraphicsResource.h"
#include "core/engine/geometry/MeshData.h"


class Buffer;

struct MeshConfiguration {
    WeakResource<vkr::Device> device;
    const void* vertices = nullptr;
    size_t vertexCount = 0;
    size_t vertexSize = 0;
    const void* indices = nullptr;
    size_t indexCount = 0;
    size_t indexSize = 0;

    template<typename Vertex_t>
    void setVertices(const std::vector<typename MeshData<Vertex_t>::Vertex>& verticesArray);

    template<typename Vertex_t>
    void setIndices(const std::vector<typename MeshData<Vertex_t>::Index>& indicesArray);

    template<typename Vertex_t>
    void setIndices(const std::vector<typename MeshData<Vertex_t>::Triangle>& triangleArray);

    template<typename Vertex_t>
    void setMeshData(MeshData<Vertex_t>* meshData);

    void setPrimitiveType(MeshPrimitiveType primitiveType);
};

class Mesh : public GraphicsResource {
    NO_COPY(Mesh);
private:
    explicit Mesh(const WeakResource<vkr::Device>& device, const std::string& name);

public:
    ~Mesh() override;

    static Mesh* create(const MeshConfiguration& meshConfiguration, const std::string& name);

    bool uploadVertices(const void* vertices, vk::DeviceSize vertexSize, size_t vertexCount);

    template<typename Vertex_t>
    bool uploadVertices(const typename MeshData<Vertex_t>::Vertex* vertices, size_t vertexCount);

    template<typename Vertex_t>
    bool uploadVertices(const std::vector<typename MeshData<Vertex_t>::Vertex>& vertices);

    bool uploadIndices(const void* indices, vk::DeviceSize indexSize, size_t indexCount);

    template<typename Vertex_t>
    bool uploadIndices(const typename MeshData<Vertex_t>::Index* indices, size_t indexCount);

    template<typename Vertex_t>
    bool uploadIndices(const std::vector<typename MeshData<Vertex_t>::Index>& indices);

    void draw(const vk::CommandBuffer& commandBuffer, uint32_t instanceCount, uint32_t firstInstance);

    void reset();

    uint32_t getVertexCount() const;

    uint32_t getIndexCount() const;

    bool hasIndices() const;

private:
    Buffer* m_vertexBuffer;
    Buffer* m_indexBuffer;
    vk::DeviceSize m_vertexSize;
    vk::DeviceSize m_indexSize;
};


template<typename Vertex_t>
void MeshConfiguration::setVertices(const std::vector<typename MeshData<Vertex_t>::Vertex>& verticesArray) {
    vertices = verticesArray.data();
    vertexCount = verticesArray.size();
    vertexSize = sizeof(typename MeshData<Vertex_t>::Vertex);
}

template<typename Vertex_t>
void MeshConfiguration::setIndices(const std::vector<typename MeshData<Vertex_t>::Index>& indicesArray) {
    indices = indicesArray.data();
    indexCount = indicesArray.size();
    indexSize = sizeof(typename MeshData<Vertex_t>::Index);
}

template<typename Vertex_t>
void MeshConfiguration::setIndices(const std::vector<typename MeshData<Vertex_t>::Triangle>& triangleArray) {
    indices = reinterpret_cast<const typename MeshData<Vertex_t>::Index*>(triangleArray.data());
    indexCount = triangleArray.size() * 3;
    indexSize = sizeof(typename MeshData<Vertex_t>::Index);
}

template<typename Vertex_t>
void MeshConfiguration::setMeshData(MeshData<Vertex_t>* meshData) {
    setVertices<Vertex_t>(meshData->getVertices());
    setIndices<Vertex_t>(meshData->getIndices());
}


template<typename Vertex_t>
bool Mesh::uploadVertices(const typename MeshData<Vertex_t>::Vertex* vertices, size_t vertexCount) {
    return uploadVertices(vertices, (vk::DeviceSize)sizeof(MeshData<Vertex_t>::Vertex), vertexCount);
}

template<typename Vertex_t>
bool Mesh::uploadVertices(const std::vector<typename MeshData<Vertex_t>::Vertex>& vertices) {
    return uploadVertices<Vertex_t>(vertices.data(), vertices.size());
}

template<typename Vertex_t>
bool Mesh::uploadIndices(const typename MeshData<Vertex_t>::Index* indices, size_t indexCount) {
    return uploadIndices(indices, (vk::DeviceSize)sizeof(MeshData<Vertex_t>::Index), indexCount);
}

template<typename Vertex_t>
bool Mesh::uploadIndices(const std::vector<typename MeshData<Vertex_t>::Index>& indices) {
    return uploadIndices<Vertex_t>(indices.data(), indices.size());
}

#endif //WORLDENGINE_MESH_H
