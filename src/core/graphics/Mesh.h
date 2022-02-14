#pragma once

#include "../core.h"

#include "GraphicsManager.h"
#include "Buffer.h"

class MeshData {
public:
	typedef uint32_t Index;


	typedef struct Vertex {
		union {
			glm::vec3 position;
			struct { float px, py, pz; };
		};
		union {
			glm::vec3 normal;
			struct { float nx, ny, nz; };
		};
		union {
			glm::vec2 texture;
			struct { float tx, ty; };
		};

		Vertex();

		Vertex(const Vertex& copy);

		Vertex(glm::vec3 position, glm::vec3 normal, glm::vec2 texture);

		Vertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty);

		Vertex operator*(const glm::mat4& m) const;

		Vertex& operator*=(const glm::mat4& m);

		static std::vector<vk::VertexInputBindingDescription> getBindingDescriptions();

		static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions();

	} Vertex;


	typedef struct Triangle {
		friend class MeshData;

		union {
			struct { Index i0, i1, i2; };
			Index indices[3];
			uint8_t bytes[sizeof(Index) * 3];
		};

		Triangle();

		Triangle(Index i0, Index i1, Index i2);

		Vertex& getVertex(MeshData& meshData, Index index) const;

		glm::vec3 getFacing(MeshData& meshData) const;

		glm::vec3 getNormal(MeshData& meshData) const;

	} Triangle;


public:
	MeshData();

	~MeshData();

	void pushTransform();

	void popTransform();

	void translate(glm::vec3 v);

	void translate(float x, float y, float z);

	void rotate(glm::quat q);

	void rotate(float angle, glm::vec3 axis);

	void rotateDegrees(float angle, glm::vec3 axis);

	void rotate(float angle, float x, float y, float z);

	void rotateDegrees(float angle, float x, float y, float z);

	void scale(glm::vec3 s);

	void scale(float x, float y, float z);

	void scale(float scale);

	void createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

	void createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, glm::vec3 normal);

	void createQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3);

	void createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 n0, glm::vec3 n1, glm::vec3 n2, glm::vec3 n3, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3);

	void createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3);

	void createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal);

	void createCuboid(glm::vec3 pos0, glm::vec3 pos1);

	Index addVertex(const Vertex& vertex);

	Index addVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture);

	Index addVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty);

	Index addTriangle(const Triangle& triangle);

	Index addTriangle(Index	i0, Index i1, Index i2);

	Index addTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

	const std::vector<Vertex>& getVertices() const;

	const std::vector<Triangle>& getTriangles() const;

private:
	std::vector<Vertex> m_vertices;
	std::vector<Triangle> m_triangles;
	std::stack<glm::mat4> m_transfromStack;
	glm::mat4 m_currentTransform;
};







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

	void draw(const vk::CommandBuffer& commandBuffer);

	uint32_t getVertexCount() const;

	uint32_t getIndexCount() const;

private:
	std::shared_ptr<vkr::Device> m_device;
	Buffer* m_vertexBuffer;
	Buffer* m_indexBuffer;
};



