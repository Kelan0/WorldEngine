
#ifndef WORLDENGINE_MESH_H
#define WORLDENGINE_MESH_H

#include "core/core.h"

#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Buffer.h"
#include "core/engine/geometry/MeshData.h"


struct MeshConfiguration {
    std::weak_ptr<vkr::Device> device;
    const void* vertices = nullptr;
    size_t vertexCount = 0;
    size_t vertexSize = 0;
    const void* indices = nullptr;
    size_t indexCount = 0;
    size_t indexSize = 0;
    MeshPrimitiveType primitiveType = PrimitiveType_Triangle;

    template<typename Vertex_t>
    void setVertices(const std::vector<typename MeshData<Vertex_t>::Vertex>& verticesArray);

    template<typename Vertex_t>
    void setIndices(const std::vector<typename MeshData<Vertex_t>::Index>& indicesArray);

    template<typename Vertex_t>
    void setIndices(const std::vector<typename MeshData<Vertex_t>::Triangle>& triangleArray);

    template<typename Vertex_t>
    void setMeshData(MeshData<Vertex_t>* meshData);

    void setPrimitiveType(const MeshPrimitiveType& primitiveType);
};

class Mesh {
    NO_COPY(Mesh);
private:
    Mesh(std::weak_ptr<vkr::Device> device);

public:
    ~Mesh();

    static Mesh* create(const MeshConfiguration& meshConfiguration);

    bool uploadVertices(const void* vertices, size_t vertexSize, size_t vertexCount);

    template<typename Vertex_t>
    bool uploadVertices(const typename MeshData<Vertex_t>::Vertex* vertices, size_t vertexCount);

    template<typename Vertex_t>
    bool uploadVertices(const std::vector<typename MeshData<Vertex_t>::Vertex>& vertices);

    bool uploadIndices(const void* indices, size_t indexSize, size_t indexCount);

    template<typename Vertex_t>
    bool uploadIndices(const typename MeshData<Vertex_t>::Index* indices, size_t indexCount);

    template<typename Vertex_t>
    bool uploadIndices(const std::vector<typename MeshData<Vertex_t>::Index>& indices);

    void setPrimitiveType(const MeshPrimitiveType& primitiveType);

    void draw(const vk::CommandBuffer& commandBuffer, uint32_t instanceCount, uint32_t firstInstance);

    uint32_t getVertexCount() const;

    uint32_t getIndexCount() const;

    const MeshPrimitiveType& getPrimitiveType() const;

    const GraphicsResource& getResourceId() const;

private:
    std::shared_ptr<vkr::Device> m_device;
    Buffer* m_vertexBuffer;
    Buffer* m_indexBuffer;
    size_t m_vertexSize;
    size_t m_indexSize;
    MeshPrimitiveType m_primitiveType;
    GraphicsResource m_resourceId;
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
    return uploadVertices(vertices, sizeof(MeshData<Vertex_t>::Vertex), vertexCount);
}

template<typename Vertex_t>
bool Mesh::uploadVertices(const std::vector<typename MeshData<Vertex_t>::Vertex>& vertices) {
    return uploadVertices<Vertex_t>(vertices.data(), vertices.size());
}

template<typename Vertex_t>
bool Mesh::uploadIndices(const typename MeshData<Vertex_t>::Index* indices, size_t indexCount) {
    return uploadIndices(indices, sizeof(MeshData<Vertex_t>::Index), indexCount);
}

template<typename Vertex_t>
bool Mesh::uploadIndices(const std::vector<typename MeshData<Vertex_t>::Index>& indices) {
    return uploadIndices<Vertex_t>(indices.data(), indices.size());
}

#endif //WORLDENGINE_MESH_H
