
#ifndef WORLDENGINE_MESHDATA_H
#define WORLDENGINE_MESHDATA_H

#include "core/core.h"


enum MeshPrimitiveType {
    PrimitiveType_Point = 0,
    PrimitiveType_Line = 1,
    PrimitiveType_LineStrip = 2, // TODO: support in MeshData building functions
    PrimitiveType_LineLoop = 3, // TODO: support in MeshData building functions
    PrimitiveType_Triangle = 4,
    PrimitiveType_TriangleStrip = 5, // TODO: support in MeshData building functions
//    PrimitiveType_Quad = 6,
};

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

} Vertex;


template<typename Vertex_t>
class MeshData {
public:
    typedef Vertex_t Vertex;
    typedef uint32_t Index;

    typedef struct Triangle {
        friend class MeshData;

        union {
            struct { Index i0, i1, i2; };
            Index indices[3];
            uint8_t bytes[sizeof(Index) * 3];
        };

        Triangle();

        Triangle(Index i0, Index i1, Index i2);

        Vertex& getVertex(std::vector<Vertex>& vertices, Index index) const;

        Vertex& getVertex(MeshData& meshData, Index index) const;

        const Vertex& getVertex(const std::vector<Vertex>& vertices, Index index) const;

        const Vertex& getVertex(const MeshData& meshData, Index index) const;

        glm::vec3 getFacing(const std::vector<Vertex>& vertices) const;

        glm::vec3 getFacing(const MeshData& meshData) const;

        glm::vec3 getNormal(const std::vector<Vertex>& vertices) const;

        glm::vec3 getNormal(const MeshData& meshData) const;

    } Triangle;

    struct Transform {
        glm::mat4 modelMatrix = glm::mat4(1.0F);
        glm::mat4 normalMatrix = glm::mat4(1.0F);
    };

    struct State {
        Index baseVertex = 0;
        Index baseIndex = 0;
    };

public:
    MeshData();

    MeshData(const MeshPrimitiveType& primitiveType);

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

    void applyTransform(bool currentStateOnly = true);

    void pushState();

    void popState();

    void reset(const MeshPrimitiveType& primitiveType);

    void clear();

    void createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

    void createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, glm::vec3 normal);

    void createQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3);

    void createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 n0, glm::vec3 n1, glm::vec3 n2, glm::vec3 n3, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3);

    void createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3);

    void createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal);

    void createCuboid(glm::vec3 pos0, glm::vec3 pos1);

    void createUVSphere(glm::vec3 center, float radius, uint32_t slices, uint32_t stacks);

    Index addVertex(const Vertex& vertex);

    Index addVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture);

    Index addVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty);

    void addTriangle(Index	i0, Index i1, Index i2);

    void addTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

    void addQuad(Index	i0, Index i1, Index i2, Index i3);

    void addQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3);

    const std::vector<Vertex>& getVertices() const;

    const std::vector<Index>& getIndices() const;

    std::vector<Vertex>& vertices();

    std::vector<Index>& indices();

    glm::vec3 calculateCenterOffset(bool currentStateOnly = true) const;

    // Transforming a vector where any component is 0 or 1 by this matrix will result in a point on the bounding box.
    // e.g. (-1,-1,-1) will be BB min, (+1,+1,+1) will be BB max, (0,+1,0) will be center top etc.
    glm::mat4 calculateBoundingBox(bool currentStateOnly = true) const;

    size_t getVertexCount() const;

    size_t getIndexCount() const;

    size_t getPolygonCount() const;

    static size_t getPolygonCount(const size_t& numIndices, const MeshPrimitiveType& primitiveType);

    const MeshPrimitiveType& getPrimitiveType() const;

private:
    Index createTrianglePrimitive(const Index& i0, const Index& i1, const Index& i2);

    Index createLinePrimitive(const Index& i0, const Index& i1);

    Index createPointPrimitive(const Index& i0);

private:
    std::vector<Vertex> m_vertices;
    std::vector<Index> m_indices;
    std::stack<glm::mat4> m_transformStack;
    glm::mat4 m_currentTransform;
    std::stack<State> m_stateStack;
    State m_currentState;

    MeshPrimitiveType m_primitiveType;
};



namespace MeshUtils {
    typedef MeshData<Vertex> OBJMeshData;

    bool loadOBJFile(const std::string& filePath, OBJMeshData& meshData);

    bool loadMeshData(const std::string& filePath, OBJMeshData& meshData);

    size_t getPolygonCount(const size_t& numIndices, const MeshPrimitiveType& primitiveType);

    template<typename Vertex_t>
    static std::vector<vk::VertexInputBindingDescription> getVertexBindingDescriptions();

    template<typename Vertex_t>
    static std::vector<vk::VertexInputAttributeDescription> getVertexAttributeDescriptions();
};



template<typename Vertex_t>
std::vector<vk::VertexInputBindingDescription> MeshUtils::getVertexBindingDescriptions() {
    std::vector<vk::VertexInputBindingDescription> inputBindingDescriptions;
    inputBindingDescriptions.resize(1);
    inputBindingDescriptions[0].setBinding(0);
    inputBindingDescriptions[0].setStride(sizeof(typename MeshData<Vertex_t>::Vertex));
    inputBindingDescriptions[0].setInputRate(vk::VertexInputRate::eVertex);
    return inputBindingDescriptions;
}

template<typename Vertex_t>
std::vector<vk::VertexInputAttributeDescription> MeshUtils::getVertexAttributeDescriptions() {
    std::vector<vk::VertexInputAttributeDescription> attribDescriptions;
    attribDescriptions.resize(3);

    attribDescriptions[0].setBinding(0);
    attribDescriptions[0].setLocation(0);
    attribDescriptions[0].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
    attribDescriptions[0].setOffset(offsetof(typename MeshData<Vertex_t>::Vertex, position));

    attribDescriptions[1].setBinding(0);
    attribDescriptions[1].setLocation(1);
    attribDescriptions[1].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
    attribDescriptions[1].setOffset(offsetof(typename MeshData<Vertex_t>::Vertex, normal));

    attribDescriptions[2].setBinding(0);
    attribDescriptions[2].setLocation(2);
    attribDescriptions[2].setFormat(vk::Format::eR32G32Sfloat); // vec2
    attribDescriptions[2].setOffset(offsetof(typename MeshData<Vertex_t>::Vertex, texture));
    return attribDescriptions;
}






template<typename Vertex_t>
MeshData<Vertex_t>::Triangle::Triangle() :
        i0(0), i1(0), i2(0) {
}

template<typename Vertex_t>
MeshData<Vertex_t>::Triangle::Triangle(Index i0, Index i1, Index i2) :
        i0(i0), i1(i1), i2(i2) {
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Vertex& MeshData<Vertex_t>::Triangle::getVertex(std::vector<Vertex>& vertices, Index index) const {
#if _DEBUG
    if (index < 0 || index >= 3) {
		printf("Get triangle vertex: internal triangle index %d is out of range [0..3]\n", index);
		assert(false);
	}
#endif

    const Index& vertexIndex = indices[index];

#if _DEBUG
    if (vertexIndex < 0 || vertexIndex >= vertices.size()) {
		printf("Triangle vertex index %d is out of range [0..%d]\n", vertexIndex, vertices.size());
		assert(false);
	}
#endif

    return vertices.at(vertexIndex);
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Vertex& MeshData<Vertex_t>::Triangle::getVertex(MeshData& meshData, Index index) const {
    return getVertex(meshData.m_vertices, index);
}

template<typename Vertex_t>
const typename MeshData<Vertex_t>::Vertex& MeshData<Vertex_t>::Triangle::getVertex(const std::vector<MeshData::Vertex>& vertices, MeshData::Index index) const {
#if _DEBUG
    if (index < 0 || index >= 3) {
		printf("Get triangle vertex: internal triangle index %d is out of range [0..3]\n", index);
		assert(false);
	}
#endif

    const Index& vertexIndex = indices[index];

#if _DEBUG
    if (vertexIndex < 0 || vertexIndex >= vertices.size()) {
		printf("Triangle vertex index %d is out of range [0..%d]\n", vertexIndex, vertices.size());
		assert(false);
	}
#endif

    return vertices.at(vertexIndex);
}

template<typename Vertex_t>
const typename MeshData<Vertex_t>::Vertex& MeshData<Vertex_t>::Triangle::getVertex(const MeshData& meshData, MeshData::Index index) const {
    return getVertex(meshData.m_vertices, index);
}

template<typename Vertex_t>
glm::vec3 MeshData<Vertex_t>::Triangle::getFacing(const std::vector<Vertex>& vertices) const {
    const glm::vec3& v0 = getVertex(vertices, 0).position;
    const glm::vec3& v1 = getVertex(vertices, 1).position;
    const glm::vec3& v2 = getVertex(vertices, 2).position;

    return glm::cross(v1 - v0, v2 - v0);
}

template<typename Vertex_t>
glm::vec3 MeshData<Vertex_t>::Triangle::getFacing(const MeshData& meshData) const {
    return getFacing(meshData.m_vertices);
}

template<typename Vertex_t>
glm::vec3 MeshData<Vertex_t>::Triangle::getNormal(const std::vector<Vertex>& vertices) const {
    return glm::normalize(getFacing(vertices));
}

template<typename Vertex_t>
glm::vec3 MeshData<Vertex_t>::Triangle::getNormal(const MeshData& meshData) const {
    return getNormal(meshData.m_vertices);
}

template<typename Vertex_t>
MeshData<Vertex_t>::MeshData():
        MeshData(PrimitiveType_Triangle) {
}

template<typename Vertex_t>
MeshData<Vertex_t>::MeshData(const MeshPrimitiveType& primitiveType) :
        m_currentTransform(1.0F),
        m_currentState(),
        m_primitiveType(primitiveType) {
}

template<typename Vertex_t>
MeshData<Vertex_t>::~MeshData() {
}

template<typename Vertex_t>
void MeshData<Vertex_t>::pushTransform() {
    m_transformStack.push(m_currentTransform);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::popTransform() {
#if _DEBUG
    if (m_transformStack.empty()) {
		printf("MeshData::popTransform(): Stack underflow\n");
		assert(false);
		return;
	}
#endif
    m_currentTransform = m_transformStack.top();
    m_transformStack.pop();
}

template<typename Vertex_t>
void MeshData<Vertex_t>::translate(glm::vec3 v) {
    m_currentTransform = glm::translate(m_currentTransform, v);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::translate(float x, float y, float z) {
    translate(glm::vec3(x, y, z));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::rotate(glm::quat q) {
    m_currentTransform = glm::mat4_cast(q) * m_currentTransform;
}

template<typename Vertex_t>
void MeshData<Vertex_t>::rotate(float angle, glm::vec3 axis) {
    m_currentTransform = glm::rotate(m_currentTransform, angle, axis);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::rotateDegrees(float angle, glm::vec3 axis) {
    rotate(glm::radians(angle), axis);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::rotate(float angle, float x, float y, float z) {
    rotate(angle, glm::vec3(x, y, z));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::rotateDegrees(float angle, float x, float y, float z) {
    rotate(glm::radians(angle), glm::vec3(x, y, z));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::scale(glm::vec3 s) {
    m_currentTransform = glm::scale(m_currentTransform, s);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::scale(float x, float y, float z) {
    scale(glm::vec3(x, y, z));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::scale(float s) {
    scale(glm::vec3(s));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::applyTransform(bool currentStateOnly) {
    size_t firstVertex = currentStateOnly ? m_currentState.baseVertex : 0;
    for (int i = firstVertex; i < m_vertices.size(); ++i) {
        m_vertices[i] *= m_currentTransform;
    }
}

template<typename Vertex_t>
void MeshData<Vertex_t>::pushState() {
    m_stateStack.push(m_currentState);
    m_currentState.baseVertex = m_vertices.size();
    m_currentState.baseIndex = m_indices.size();
}

template<typename Vertex_t>
void MeshData<Vertex_t>::popState() {
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

template<typename Vertex_t>
void MeshData<Vertex_t>::reset(const MeshPrimitiveType &primitiveType) {
    clear();
    m_primitiveType = primitiveType;
}

template<typename Vertex_t>
void MeshData<Vertex_t>::clear() {
    m_currentTransform = glm::mat4(1.0F);
    m_currentState = State();

    while (!m_stateStack.empty())
        m_stateStack.pop();

    while (!m_transformStack.empty())
        m_transformStack.pop();

    m_vertices.clear();
    m_indices.clear();
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    Index i0 = addVertex(v0);
    Index i1 = addVertex(v1);
    Index i2 = addVertex(v2);
    addTriangle(i0, i1, i2);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, glm::vec3 normal) {
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

template<typename Vertex_t>
void MeshData<Vertex_t>::createQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    addQuad(v0, v1, v2, v3);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 n0, glm::vec3 n1, glm::vec3 n2, glm::vec3 n3, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3) {
    createQuad(Vertex(p0, n0, t0), Vertex(p1, n1, t1), Vertex(p2, n2, t2), Vertex(p3, n3, t3));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal, glm::vec2 t0, glm::vec2 t1, glm::vec2 t2, glm::vec2 t3) {
    createQuad(p0, p1, p2, p3, normal, normal, normal, normal, t0, t1, t2, t3);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal) {
    createQuad(p0, p1, p2, p3, normal, normal, normal, normal, glm::vec2(0.0F, 0.0F), glm::vec2(1.0F, 0.0F), glm::vec2(1.0F, 1.0F), glm::vec2(0.0F, 1.0F));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createCuboid(glm::vec3 pos0, glm::vec3 pos1) {
    createQuad(glm::vec3(pos0.x, pos0.y, pos0.z), glm::vec3(pos0.x, pos0.y, pos1.z), glm::vec3(pos0.x, pos1.y, pos1.z), glm::vec3(pos0.x, pos1.y, pos0.z), glm::vec3(-1.0F, 0.0F, 0.0F));
    createQuad(glm::vec3(pos1.x, pos0.y, pos1.z), glm::vec3(pos1.x, pos0.y, pos0.z), glm::vec3(pos1.x, pos1.y, pos0.z), glm::vec3(pos1.x, pos1.y, pos1.z), glm::vec3(+1.0F, 0.0F, 0.0F));
    createQuad(glm::vec3(pos0.x, pos0.y, pos0.z), glm::vec3(pos1.x, pos0.y, pos0.z), glm::vec3(pos1.x, pos0.y, pos1.z), glm::vec3(pos0.x, pos0.y, pos1.z), glm::vec3(0.0F, -1.0F, 0.0F));
    createQuad(glm::vec3(pos1.x, pos1.y, pos0.z), glm::vec3(pos0.x, pos1.y, pos0.z), glm::vec3(pos0.x, pos1.y, pos1.z), glm::vec3(pos1.x, pos1.y, pos1.z), glm::vec3(0.0F, +1.0F, 0.0F));
    createQuad(glm::vec3(pos1.x, pos0.y, pos0.z), glm::vec3(pos0.x, pos0.y, pos0.z), glm::vec3(pos0.x, pos1.y, pos0.z), glm::vec3(pos1.x, pos1.y, pos0.z), glm::vec3(0.0F, 0.0F, -1.0F));
    createQuad(glm::vec3(pos0.x, pos0.y, pos1.z), glm::vec3(pos1.x, pos0.y, pos1.z), glm::vec3(pos1.x, pos1.y, pos1.z), glm::vec3(pos0.x, pos1.y, pos1.z), glm::vec3(0.0F, 0.0F, +1.0F));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createUVSphere(glm::vec3 center, float radius, uint32_t slices, uint32_t stacks) {
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

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::addVertex(const Vertex& vertex) {
    size_t index = m_vertices.size() - m_currentState.baseVertex;
    m_vertices.emplace_back(vertex * m_currentTransform);
    return static_cast<Index>(index);
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::addVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture) {
    return addVertex(Vertex(position, normal, texture));
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::addVertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty) {
    return addVertex(Vertex(px, py, pz, nx, ny, nz, tx, ty));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::addTriangle(Index i0, Index i1, Index i2) {
    switch (m_primitiveType) {
        case PrimitiveType_Triangle:
            createTrianglePrimitive(i0, i1, i2);
            break;
        case PrimitiveType_Line:
            createLinePrimitive(i0, i1);
            createLinePrimitive(i1, i2);
            createLinePrimitive(i2, i0);
            break;
        case PrimitiveType_Point:
            createPointPrimitive(i0);
            createPointPrimitive(i1);
            createPointPrimitive(i2);
            break;
    }
}

template<typename Vertex_t>
void MeshData<Vertex_t>::addTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    Index i0 = addVertex(v0);
    Index i1 = addVertex(v1);
    Index i2 = addVertex(v2);
    addTriangle(i0, i1, i2);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::addQuad(Index i0, Index i1, Index i2, Index i3) {
    switch (m_primitiveType) {
        case PrimitiveType_Triangle:
            createTrianglePrimitive(i0, i1, i2);
            createTrianglePrimitive(i0, i2, i3);
            break;
        case PrimitiveType_Line:
            createLinePrimitive(i0, i1);
            createLinePrimitive(i1, i2);
            createLinePrimitive(i2, i3);
            createLinePrimitive(i3, i0);
            break;
        case PrimitiveType_Point:
            createPointPrimitive(i0);
            createPointPrimitive(i1);
            createPointPrimitive(i2);
            createPointPrimitive(i3);
            break;
    }
}

template<typename Vertex_t>
void MeshData<Vertex_t>::addQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    Index i0 = addVertex(v0);
    Index i1 = addVertex(v1);
    Index i2 = addVertex(v2);
    Index i3 = addVertex(v3);
    addQuad(i0, i1, i2, i3);
}

template<typename Vertex_t>
const std::vector<typename MeshData<Vertex_t>::Vertex>& MeshData<Vertex_t>::getVertices() const {
    return m_vertices;
}

template<typename Vertex_t>
const std::vector<typename MeshData<Vertex_t>::Index>& MeshData<Vertex_t>::getIndices() const {
    return m_indices;
}

template<typename Vertex_t>
std::vector<typename MeshData<Vertex_t>::Vertex>& MeshData<Vertex_t>::vertices() {
    return m_vertices;
}

template<typename Vertex_t>
std::vector<typename MeshData<Vertex_t>::Index>& MeshData<Vertex_t>::indices() {
    return m_indices;
}

template<typename Vertex_t>
glm::vec3 MeshData<Vertex_t>::calculateCenterOffset(bool currentStateOnly) const {
    glm::dvec3 center(0.0F);

    uint32_t bucketCount = 0;

    // We calculate the average position of all vertices in the mesh (or just the vertices in the current state)
    // Positions are averaged in buckets to avoid floating point precision issues.

    const uint32_t maxBucketSize = 1000;
    uint32_t bucketSize = 0;
    glm::dvec3 bucketCenter = glm::dvec3(0.0F);

    const size_t firstIndex = currentStateOnly ? m_currentState.baseIndex : 0;

    for (int i = firstIndex; i < m_indices.size(); ++i) {
        bucketCenter += m_vertices[m_indices[i]].position;
        ++bucketSize;
        if (bucketSize == maxBucketSize) {
            center += bucketCenter / (double)bucketSize;
            ++bucketCount;
            bucketSize = 0;
            bucketCenter = glm::dvec3(0.0);
        }
//        bucketCenter += m_triangles[i].getVertex(m_vertices, 0).position;
//        bucketCenter += m_triangles[i].getVertex(m_vertices, 1).position;
//        bucketCenter += m_triangles[i].getVertex(m_vertices, 2).position;
//        ++bucketSize;
//
//        if (bucketSize == maxBucketSize) {
//            center += bucketCenter / (bucketSize * 3.0);
//            ++bucketCount;
//            bucketSize = 0;
//            bucketCenter = glm::dvec3(0.0);
//        }
    }

    return center / (double)bucketCount;
}

template<typename Vertex_t>
glm::mat4 MeshData<Vertex_t>::calculateBoundingBox(bool currentStateOnly) const {
    glm::vec3 minExtent = glm::vec3(+INFINITY);
    glm::vec3 maxExtent = glm::vec3(-INFINITY);

    const size_t firstPrimitive = currentStateOnly ? m_currentState.baseIndex : 0;

    for (int i = firstPrimitive; i < m_indices.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            const glm::vec3& pos = m_vertices[m_indices[i]].position;
//            const glm::vec3& pos = m_triangles[i].getVertex(m_vertices, 0).position;

            minExtent = glm::min(minExtent, pos);
            maxExtent = glm::max(maxExtent, pos);
        }
    }

    glm::vec3 halfExtent = (maxExtent - minExtent) * 0.5F;
    glm::vec3 center = (maxExtent + minExtent) * 0.5F;

    glm::mat4 bbMat(1.0F);
    bbMat[0] *= halfExtent.x;
    bbMat[1] *= halfExtent.y;
    bbMat[2] *= halfExtent.z;
    bbMat[3] = glm::vec4(center, 1.0F);
    return bbMat;
}

template<typename Vertex_t>
size_t MeshData<Vertex_t>::getVertexCount() const {
    return m_vertices.size();
}

template<typename Vertex_t>
size_t MeshData<Vertex_t>::getIndexCount() const {
    return m_indices.size();
}

template<typename Vertex_t>
size_t MeshData<Vertex_t>::getPolygonCount() const {
    return MeshUtils::getPolygonCount(m_indices.size(), m_primitiveType);
}

template<typename Vertex_t>
const MeshPrimitiveType& MeshData<Vertex_t>::getPrimitiveType() const {
    return m_primitiveType;
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::createTrianglePrimitive(const Index& i0, const Index& i1, const Index& i2) {
    assert(m_primitiveType == PrimitiveType_Triangle);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    m_indices.emplace_back(i1 + m_currentState.baseVertex);
    m_indices.emplace_back(i2 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::createLinePrimitive(const Index& i0, const Index& i1) {
    assert(m_primitiveType == PrimitiveType_Line);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    m_indices.emplace_back(i1 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::createPointPrimitive(const Index& i0) {
    assert(m_primitiveType == PrimitiveType_Point);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}

#endif //WORLDENGINE_MESHDATA_H
