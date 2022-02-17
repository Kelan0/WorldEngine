#include "Mesh.h"
#include "GPUMemory.h"
#include "../application/Application.h"








void MeshConfiguration::setVertices(const std::vector<MeshData::Vertex>& verticesArray) {
	vertices = verticesArray.data();
	vertexCount = verticesArray.size();
}

void MeshConfiguration::setIndices(const std::vector<MeshData::Index>& indicesArray) {
	indices = indicesArray.data();
	indexCount = indicesArray.size();
}

void MeshConfiguration::setIndices(const std::vector<MeshData::Triangle>& triangleArray) {
	indices = reinterpret_cast<const MeshData::Index*>(triangleArray.data());
	indexCount = triangleArray.size() * 3;
}

void MeshConfiguration::setMeshData(MeshData* meshData) {
	setVertices(meshData->getVertices());
	setIndices(meshData->getTriangles());
}



Mesh::Mesh(std::weak_ptr<vkr::Device> device):
	m_device(device) {
}

Mesh::~Mesh() {
	delete m_vertexBuffer;
	delete m_indexBuffer;
}

Mesh* Mesh::create(const MeshConfiguration& meshConfiguration) {

	Mesh* mesh = new Mesh(meshConfiguration.device);
	
	if (meshConfiguration.vertexCount > 0) {
		if (!mesh->uploadVertices(meshConfiguration.vertices, meshConfiguration.vertexCount)) {
			printf("Unable to create mesh, failed to upload vertices\n");
			delete mesh;
			return NULL;
		}
	}
	
	if (meshConfiguration.indexCount > 0) {
		if (!mesh->uploadIndices(meshConfiguration.indices, meshConfiguration.indexCount)) {
			printf("Unable to create mesh, failed to upload indices\n");
			delete mesh;
			return NULL;
		}
	}

	return mesh;
}

bool Mesh::uploadVertices(const MeshData::Vertex* vertices, size_t vertexCount) {
	delete m_vertexBuffer;

	if (vertexCount <= 0) {
		// Valid to pass no vertices, we just deleted the buffer
		return true;
	}

	assert(vertices != NULL);

	BufferConfiguration vertexBufferConfig;
	vertexBufferConfig.device = m_device;
	vertexBufferConfig.size = vertexCount * sizeof(MeshData::Vertex);
	vertexBufferConfig.data = (void*)vertices;
	vertexBufferConfig.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	vertexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	m_vertexBuffer = Buffer::create(vertexBufferConfig);

	if (m_vertexBuffer == NULL) {
		printf("Failed to create vertex buffer\n");
		return false;
	}

	return true;
}

bool Mesh::uploadVertices(const std::vector<MeshData::Vertex>& vertices) {
	return uploadVertices(vertices.data(), vertices.size());
}

bool Mesh::uploadIndices(const MeshData::Index* indices, size_t indexCount) {
	delete m_indexBuffer;

	if (indexCount <= 0) {
		// Valid to pass no vertices, we just deleted the buffer
		return true;
	}

	BufferConfiguration indexBufferConfig;
	indexBufferConfig.device = m_device;
	indexBufferConfig.size = indexCount * sizeof(MeshData::Index);
	indexBufferConfig.data = (void*)indices;
	indexBufferConfig.usage = vk::BufferUsageFlagBits::eIndexBuffer;
	indexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	m_indexBuffer = Buffer::create(indexBufferConfig);

	if (m_indexBuffer == NULL) {
		printf("Failed to create vertex buffer\n");
		return false;
	}

	return true;
}

bool Mesh::uploadIndices(const std::vector<MeshData::Index>& indices) {
	return uploadIndices(indices.data(), indices.size());
}

void Mesh::draw(const vk::CommandBuffer& commandBuffer) {
	const vk::Buffer& vertexBuffer = m_vertexBuffer->getBuffer();
	vk::DeviceSize offset = 0;

	commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer, &offset);
	commandBuffer.bindIndexBuffer(m_indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

	commandBuffer.drawIndexed(getIndexCount(), 1, 0, 0, 0);
}

uint32_t Mesh::getVertexCount() const {
	return m_vertexBuffer->getSize() / sizeof(MeshData::Vertex);
}

uint32_t Mesh::getIndexCount() const {
	return m_indexBuffer->getSize() / sizeof(MeshData::Index);
}