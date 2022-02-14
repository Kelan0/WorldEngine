#include "Mesh.h"
#include "GPUMemory.h"
#include "../application/Application.h"


#if _DEBUG

#define VALIDATE_MAX_INDEX(i) \
if (i > std::numeric_limits<Index>::max()) { \
	printf("Index %llu is larger than the maximum supported index %llu\n", (uint64_t)i, (uint64_t)std::numeric_limits<Index>::max()); \
	assert(false); \
}

#define VALIDATE_VERTEX_INDEX(i) \
if (i < 0 || i >= m_vertices.size()) { \
	printf("Vertex index " #i " = %llu is out of range\n", (uint64_t)i); \
	assert(false); \
}

#else

#define VALIDATE_MAX_INDEX
#define VALIDATE_VERTEX_INDEX

#endif

MeshData::Vertex::Vertex():
	position(0.0F),
	normal(0.0F), 
	texture(0.0F) {
}

MeshData::Vertex::Vertex(const Vertex& copy):
	position(copy.position),
	normal(copy.normal),
	texture(copy.texture) {
}

MeshData::Vertex::Vertex(glm::vec3 position, glm::vec3 normal, glm::vec2 texture):
	position(position),
	normal(normal),
	texture(texture) {
}

MeshData::Vertex::Vertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty):
	position(px, py, pz),
	normal(nx, ny, nz),
	texture(tx, ty) {
}

MeshData::Vertex MeshData::Vertex::operator*(const glm::mat4& m) const {
	Vertex v(*this);
	v *= m;
	return v;
}

MeshData::Vertex& MeshData::Vertex::operator*=(const glm::mat4& m) {
	glm::mat4 nm = glm::transpose(glm::inverse(m));
	position = glm::vec3(m * glm::vec4(position, 1.0F));
	normal = glm::vec3(nm * glm::vec4(normal, 0.0F));
	return *this;
}

std::vector<vk::VertexInputBindingDescription> MeshData::Vertex::getBindingDescriptions() {
	std::vector<vk::VertexInputBindingDescription> inputBindingDescriptions;
	inputBindingDescriptions.resize(1);
	inputBindingDescriptions[0].setBinding(0);
	inputBindingDescriptions[0].setStride(sizeof(Vertex));
	inputBindingDescriptions[0].setInputRate(vk::VertexInputRate::eVertex);
	return inputBindingDescriptions;
}

std::vector<vk::VertexInputAttributeDescription> MeshData::Vertex::getAttributeDescriptions() {
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

MeshData::Triangle::Triangle() :
	i0(0), i1(0), i2(0) {
}

MeshData::Triangle::Triangle(Index i0, Index i1, Index i2) :
	i0(i0), i1(i1), i2(i2) {
}

MeshData::Vertex& MeshData::Triangle::getVertex(MeshData& meshData, Index index) const {
#if _DEBUG
	if (index < 0 || index >= 3) {
		printf("Get triangle vertex: internal triangle index %d is out of range [0..3]\n", index);
		assert(false);
	}
#endif
	
	const Index& vertexIndex = indices[index];

#if _DEBUG
	if (vertexIndex <= 0 || vertexIndex >= meshData.m_vertices.size()) {
		printf("Triangle vertex index %d is out of range [0..%d]\n", vertexIndex, meshData.m_vertices.size());
		assert(false);
	}
#endif

	return meshData.m_vertices.at(vertexIndex);
}

glm::vec3 MeshData::Triangle::getFacing(MeshData& meshData) const {
	const glm::vec3& v0 = getVertex(meshData, 0).position;
	const glm::vec3& v1 = getVertex(meshData, 1).position;
	const glm::vec3& v2 = getVertex(meshData, 2).position;

	return glm::cross(v1 - v0, v2 - v0);
}

glm::vec3 MeshData::Triangle::getNormal(MeshData& meshData) const {
	return glm::normalize(getFacing(meshData));
}





MeshData::MeshData():
	m_currentTransform(1.0F),
	m_currentState() {
}

MeshData::~MeshData() {
}

void MeshData::pushTransform() {
	m_transfromStack.push(m_currentTransform);
}

void MeshData::popTransform() {
#if _DEBUG
	if (m_transfromStack.empty()) {
		printf("MeshData::popTransform(): Stack underflow\n");
		assert(false);
		return;
	}
#endif
	m_currentTransform = m_transfromStack.top();
	m_transfromStack.pop();
}

void MeshData::translate(glm::vec3 v) {
	m_currentTransform = glm::translate(m_currentTransform, v);
}

void MeshData::translate(float x, float y, float z) {
	translate(glm::vec3(x, y, z));
}

void MeshData::rotate(glm::quat q) {
	m_currentTransform = glm::mat4_cast(q) * m_currentTransform;
}

void MeshData::rotate(float angle, glm::vec3 axis) {
	m_currentTransform = glm::rotate(m_currentTransform, angle, axis);
}

void MeshData::rotateDegrees(float angle, glm::vec3 axis) {
	rotate(glm::radians(angle), axis);
}

void MeshData::rotate(float angle, float x, float y, float z) {
	rotate(angle, glm::vec3(x, y, z));
}

void MeshData::rotateDegrees(float angle, float x, float y, float z) {
	rotate(glm::radians(angle), glm::vec3(x, y, z));
}

void MeshData::scale(glm::vec3 s) {
	m_currentTransform = glm::scale(m_currentTransform, s);
}

void MeshData::scale(float x, float y, float z) {
	scale(glm::vec3(x, y, z));
}

void MeshData::scale(float s) {
	scale(glm::vec3(s));
}

void MeshData::pushState() {
	m_stateStack.push(m_currentState);
	m_currentState.baseVertex = m_vertices.size();
	m_currentState.baseTriangle = m_triangles.size();
}

void MeshData::popState() {
#if _DEBUG
	if (m_stateStack.empty()) {
		printf("MeshData::popState(): Stack underflow\n");
		assert(false);
		return;
	}
#endif
	m_currentState = m_stateStack.top();
	m_stateStack.pop();
}

void MeshData::createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
	Index i0 = addVertex(v0);
	Index i1 = addVertex(v1);
	Index i2 = addVertex(v2);
	addTriangle(i0, i1, i2);
}

void MeshData::createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, glm::vec3 normal) {
	Index i0 = addVertex(v0);
	Index i1 = addVertex(v1);
	Index i2 = addVertex(v2);

	Triangle t(i0, i1, i2);
	if (glm::dot(t.getFacing(*this), normal) < 0) {
		addTriangle(i0, i2, i1);
	} else {
		addTriangle(i0, i1, i2);
	}
}

void MeshData::createQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
	Index i0 = addVertex(v0);
	Index i1 = addVertex(v1);
	Index i2 = addVertex(v2);
	Index i3 = addVertex(v3);

	addTriangle(i0, i1, i2);
	addTriangle(i0, i2, i3);
}

void MeshData::createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 n0, glm::vec3 n1, glm::vec3 n2, glm::vec3 n3, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3) {
	createQuad(Vertex(p0, n0, t0), Vertex(p1, n1, t1), Vertex(p2, n2, t2), Vertex(p3, n3, t3));
}

void MeshData::createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3) {
	createQuad(p0, p1, p2, p3, normal, normal, normal, normal, t0, t1, t2, t3);
}

void MeshData::createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal) {
	createQuad(p0, p1, p2, p3, normal, normal, normal, normal, glm::vec2(0.0F, 0.0F), glm::vec2(1.0F, 0.0F), glm::vec2(1.0F, 1.0F), glm::vec2(0.0F, 1.0F));
}

void MeshData::createCuboid(glm::vec3 pos0, glm::vec3 pos1) {
	createQuad(glm::vec3(pos0.x, pos0.y, pos0.z), glm::vec3(pos0.x, pos0.y, pos1.z), glm::vec3(pos0.x, pos1.y, pos1.z), glm::vec3(pos0.x, pos1.y, pos0.z), glm::vec3(-1.0F, 0.0F, 0.0F));
	createQuad(glm::vec3(pos1.x, pos0.y, pos1.z), glm::vec3(pos1.x, pos0.y, pos0.z), glm::vec3(pos1.x, pos1.y, pos0.z), glm::vec3(pos1.x, pos1.y, pos1.z), glm::vec3(+1.0F, 0.0F, 0.0F));
	createQuad(glm::vec3(pos0.x, pos0.y, pos0.z), glm::vec3(pos1.x, pos0.y, pos0.z), glm::vec3(pos1.x, pos0.y, pos1.z), glm::vec3(pos0.x, pos0.y, pos1.z), glm::vec3(0.0F, -1.0F, 0.0F));	
	createQuad(glm::vec3(pos1.x, pos1.y, pos0.z), glm::vec3(pos0.x, pos1.y, pos0.z), glm::vec3(pos0.x, pos1.y, pos1.z), glm::vec3(pos1.x, pos1.y, pos1.z), glm::vec3(0.0F, +1.0F, 0.0F));
	createQuad(glm::vec3(pos1.x, pos0.y, pos0.z), glm::vec3(pos0.x, pos0.y, pos0.z), glm::vec3(pos0.x, pos1.y, pos0.z), glm::vec3(pos1.x, pos1.y, pos0.z), glm::vec3(0.0F, 0.0F, -1.0F));
	createQuad(glm::vec3(pos0.x, pos0.y, pos1.z), glm::vec3(pos1.x, pos0.y, pos1.z), glm::vec3(pos1.x, pos1.y, pos1.z), glm::vec3(pos0.x, pos1.y, pos1.z), glm::vec3(0.0F, 0.0F, +1.0F));
}

void MeshData::createUVSphere(glm::vec3 center, float radius, uint32_t slices, uint32_t stacks) {
	pushState();

	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 tex;

	for (int i = 0; i <= stacks; ++i) {
		tex.y = (float)(i) / stacks;
		float phi = glm::pi<float>() * (tex.y - 0.5F); // -90 to +90 degrees
		norm.y = glm::sin(phi);
		pos.y = center.y + norm.y * radius;

		for (int j = 0; j <= slices; ++j) {
			tex.x = (float)(j) / slices;
			float theta = glm::two_pi<float>() * tex.x; // 0 to 360 degrees

			norm.x = glm::cos(phi) * glm::sin(theta);
			norm.z = glm::cos(phi) * glm::cos(theta);
			pos.x = center.x + norm.x * radius;
			pos.z = center.z + norm.z * radius;

			addVertex(pos, norm, tex);

		}
	}

	for (int i0 = 0; i0 < stacks; ++i0) {
		int i1 = (i0 + 1);

		for (int j0 = 0; j0 < slices; ++j0) {
			int j1 = (j0 + 1);

			Index i00 = i0 * (slices + 1) + j0;
			Index i10 = i0 * (slices + 1) + j1;
			Index i01 = i1 * (slices + 1) + j0;
			Index i11 = i1 * (slices + 1) + j1;

			addQuad(i00, i10, i11, i01);
		}
	}

	//for (int i = 0; i <= stacks; ++i) {
	//	for (int j = 0; j <= slices; ++j) {
	//		if (i > 0) {
	//			int j1 = (j + 1) % slices;
	//			int i0 = (i - 1);
	//			Index i00 = i0 * (slices + 1) + j;
	//			Index i10 = i0 * (slices + 1) + j1;
	//			Index i01 = i * (slices + 1) + j;
	//			Index i11 = i * (slices + 1) + j1;
	//
	//			addQuad(i00, i10, i11, i01);
	//		}
	//	}
	//}

	popState();
}

MeshData::Index MeshData::addVertex(const Vertex& vertex) {
	size_t index = m_vertices.size() - m_currentState.baseVertex;
	VALIDATE_MAX_INDEX(index);
	m_vertices.emplace_back(vertex * m_currentTransform);
	return static_cast<Index>(index);
}

MeshData::Index MeshData::addVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture) {
	return addVertex(Vertex(position, normal, texture));
}

MeshData::Index MeshData::addVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty) {
	return addVertex(Vertex(px, py, pz, nx, ny, nz, tx, ty));
}

MeshData::Index MeshData::addTriangle(Index i0, Index i1, Index i2) {
	i0 += m_currentState.baseVertex;
	i1 += m_currentState.baseVertex;
	i2 += m_currentState.baseVertex;

	VALIDATE_VERTEX_INDEX(i0);
	VALIDATE_VERTEX_INDEX(i1);
	VALIDATE_VERTEX_INDEX(i2);
	size_t index = m_triangles.size() - m_currentState.baseTriangle;
	m_triangles.emplace_back(i0, i1, i2);
	return static_cast<Index>(index);
}

MeshData::Index MeshData::addTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
	Index i0 = addVertex(v0);
	Index i1 = addVertex(v1);
	Index i2 = addVertex(v2);
	return addTriangle(i0, i1, i2);
}

MeshData::Index MeshData::addQuad(Index i0, Index i1, Index i2, Index i3) {
	Index index = addTriangle(i0, i1, i2);
	addTriangle(i0, i2, i3);
	return index;
}

MeshData::Index MeshData::addQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
	Index i0 = addVertex(v0);
	Index i1 = addVertex(v1);
	Index i2 = addVertex(v2);
	Index i3 = addVertex(v3);
	return addQuad(i0, i1, i2, i3);
}

const std::vector<MeshData::Vertex>& MeshData::getVertices() const {
	return m_vertices;
}

const std::vector<MeshData::Triangle>& MeshData::getTriangles() const {
	return m_triangles;
}






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
