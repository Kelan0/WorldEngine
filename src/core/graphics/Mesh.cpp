#include "Mesh.h"
#include "GPUMemory.h"
#include "../Application.h"


std::vector<vk::VertexInputBindingDescription> Vertex::getBindingDescriptions() {
	std::vector<vk::VertexInputBindingDescription> inputBindingDescriptions;
	inputBindingDescriptions.resize(1);
	inputBindingDescriptions[0].setBinding(0);
	inputBindingDescriptions[0].setStride(sizeof(Vertex));
	inputBindingDescriptions[0].setInputRate(vk::VertexInputRate::eVertex);
	return inputBindingDescriptions;
}

std::vector<vk::VertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
	std::vector<vk::VertexInputAttributeDescription> attribDescriptions;
	attribDescriptions.resize(3);

	attribDescriptions[0].setBinding(0);
	attribDescriptions[0].setLocation(0);
	attribDescriptions[0].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
	attribDescriptions[0].setOffset(offsetof(Vertex, position));

	attribDescriptions[1].setBinding(0);
	attribDescriptions[1].setLocation(1);
	attribDescriptions[1].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
	attribDescriptions[1].setOffset(offsetof(Vertex, normal));

	attribDescriptions[2].setBinding(0);
	attribDescriptions[2].setLocation(2);
	attribDescriptions[2].setFormat(vk::Format::eR32G32Sfloat); // vec2
	attribDescriptions[2].setOffset(offsetof(Vertex, texture));
	return attribDescriptions;
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
	
	if (!meshConfiguration.vertices.empty()) {
		if (!mesh->uploadVertices(meshConfiguration.vertices)) {
			printf("Unable to create mesh, failed to upload vertices\n");
			delete mesh;
			return NULL;
		}
	}
	
	if (!meshConfiguration.indices.empty()) {
		if (!mesh->uploadIndices(meshConfiguration.indices)) {
			printf("Unable to create mesh, failed to upload indices\n");
			delete mesh;
			return NULL;
		}
	}

	return mesh;
}

bool Mesh::uploadVertices(const std::vector<Vertex>& vertices) {
	delete m_vertexBuffer;

	if (vertices.empty()) {
		// Valid to pass no vertices, we just deleted the buffer
		return true;
	}

	BufferConfiguration vertexBufferConfig;
	vertexBufferConfig.device = m_device;
	vertexBufferConfig.size = vertices.size() * sizeof(Vertex);
	vertexBufferConfig.data = (void*)&vertices[0];
	vertexBufferConfig.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	vertexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	m_vertexBuffer = Buffer::create(vertexBufferConfig);
	
	if (m_vertexBuffer == NULL) {
		printf("Failed to create vertex buffer\n");
		return false;
	}

	return true;
}

bool Mesh::uploadIndices(const std::vector<uint32_t>& indices) {
	delete m_indexBuffer;

	if (indices.empty()) {
		// Valid to pass no vertices, we just deleted the buffer
		return true;
	}

	BufferConfiguration indexBufferConfig;
	indexBufferConfig.device = m_device;
	indexBufferConfig.size = indices.size() * sizeof(uint32_t);
	indexBufferConfig.data = (void*)&indices[0];
	indexBufferConfig.usage = vk::BufferUsageFlagBits::eIndexBuffer;
	indexBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	m_indexBuffer = Buffer::create(indexBufferConfig);

	if (m_indexBuffer == NULL) {
		printf("Failed to create vertex buffer\n");
		return false;
	}

	return true;
}

void Mesh::draw(const vk::CommandBuffer& commandBuffer) {
	const vk::Buffer& vertexBuffer = m_vertexBuffer->getBuffer();
	vk::DeviceSize offset = 0;

	commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer, &offset);
	commandBuffer.bindIndexBuffer(m_indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

	commandBuffer.drawIndexed(getIndexCount(), 1, 0, 0, 0);
}

uint32_t Mesh::getVertexCount() const {
	return m_vertexBuffer->getSize() / sizeof(Vertex);
}

uint32_t Mesh::getIndexCount() const {
	return m_indexBuffer->getSize() / sizeof(uint32_t);
}

