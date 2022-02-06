#pragma once

#include "../core.h"

#include "GraphicsManager.h"
#include "Buffer.h"

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texture;

	static std::vector<vk::VertexInputBindingDescription> getBindingDescriptions();

	static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();
};

struct MeshConfiguration {
	std::shared_ptr<vkr::Device> device;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

class Mesh {
	NO_COPY(Mesh);
private:
	Mesh(std::shared_ptr<vkr::Device> device);

public:
	~Mesh();

	static Mesh* create(const MeshConfiguration& meshConfiguration);

	bool uploadVertices(const std::vector<Vertex>& vertices);

	bool uploadIndices(const std::vector<uint32_t>& indices);

	void draw(const vk::CommandBuffer& commandBuffer);

	uint32_t getVertexCount() const;

	uint32_t getIndexCount() const;

private:
	std::shared_ptr<vkr::Device> m_device;
	Buffer* m_vertexBuffer;
	Buffer* m_indexBuffer;
};