
#ifndef WORLDENGINE_MESHDATA_H
#define WORLDENGINE_MESHDATA_H

#include "core/core.h"
#include "core/util/Logger.h"


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
        glm::vec3 tangent;
        struct { float tx, ty, tz; };
    };
    union {
        glm::vec2 texture;
        struct { float tu, tv; };
    };

    Vertex();

    Vertex(const Vertex& copy);

    Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture);

    Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec3& tangent, const glm::vec2& texture);

    Vertex(float px, float py, float pz, float nx, float ny, float nz, float tu, float tv);

    Vertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, float tz, float tu, float tv);

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

    explicit MeshData(MeshPrimitiveType primitiveType);

    ~MeshData();

    void pushTransform();

    void popTransform();

    void translate(const glm::vec3& v);

    void translate(float x, float y, float z);

    void rotate(const glm::quat& q);

    void rotate(float angle, const glm::vec3& axis);

    void rotateDegrees(float angle, const glm::vec3& axis);

    void rotate(float angle, float x, float y, float z);

    void rotateDegrees(float angle, float x, float y, float z);

    void scale(const glm::vec3& s);

    void scale(float x, float y, float z);

    void scale(float scale);

    void applyTransform(bool currentStateOnly = true);

    void pushState();

    void popState();

    void reset(MeshPrimitiveType primitiveType);

    void clear();

    void createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

    void createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, const glm::vec3& normal);

    void createQuad(const Vertex& v00, const Vertex& v01, const Vertex& v11, const Vertex& v10);

    void createQuad(const glm::vec3& p00, const glm::vec3& p01, const glm::vec3& p11, const glm::vec3& p10, const glm::vec3& n00, const glm::vec3& n01, const glm::vec3& n11, const glm::vec3& n10, const glm::vec2& t00, const glm::vec2& t01, const glm::vec2& t11, const glm::vec2& t10);

    void createQuad(const glm::vec3& p00, const glm::vec3& p01, const glm::vec3& p11, const glm::vec3& p10, const glm::vec3& normal, const glm::vec2& t00, const glm::vec2& t01, const glm::vec2& t11, const glm::vec2& t10);

    void createQuad(const glm::vec3& p00, const glm::vec3& p01, const glm::vec3& p11, const glm::vec3& p10, const glm::vec3& normal);

    void createPlane(const glm::vec3& origin, const glm::vec3& u, const glm::vec3& v, const glm::vec3& normal, const glm::vec2& scale, const glm::uvec2& subdivisions);

    void createDisc(const glm::vec3& center, const glm::vec3& u, const glm::vec3& v, const glm::vec3& normal, float radius, uint32_t segments);

    void createCuboid(const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& tex0, const glm::vec3& tex1);

    void createCuboid(const glm::vec3& pos0, const glm::vec3& pos1);

    void createUVSphere(const glm::vec3& center, float radius, uint32_t slices, uint32_t stacks);

    Index addVertex(const Vertex& vertex);

    Index addVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture);

    Index addVertex(float px, float py, float pz, float nx, float ny, float nz, float tu, float tv);

    void addTriangle(Index i0, Index i1, Index i2);

    void addTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

    void addQuad(Index i0, Index i1, Index i2, Index i3);

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

    static size_t getPolygonCount(size_t numIndices, MeshPrimitiveType primitiveType);

    MeshPrimitiveType getPrimitiveType() const;

    void computeTangents();

    static void computeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, MeshPrimitiveType primitiveType);

private:
    Index createTrianglePrimitive(Index i0, Index i1, Index i2);

    Index createLinePrimitive(Index i0, Index i1);

    Index createPointPrimitive(Index i0);

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

    size_t getPolygonCount(size_t numIndices, MeshPrimitiveType primitiveType);

    template<typename Vertex_t>
    static std::vector<vk::VertexInputBindingDescription> getVertexBindingDescriptions();

    template<typename Vertex_t>
    static std::vector<vk::VertexInputAttributeDescription> getVertexAttributeDescriptions();
};



// Default overload - provide template specialization when needed.
template<typename Vertex_t>
std::vector<vk::VertexInputBindingDescription> MeshUtils::getVertexBindingDescriptions() {
    std::vector<vk::VertexInputBindingDescription> inputBindingDescriptions;
    inputBindingDescriptions.resize(1);
    inputBindingDescriptions[0].setBinding(0);
    inputBindingDescriptions[0].setStride(sizeof(typename MeshData<Vertex_t>::Vertex));
    inputBindingDescriptions[0].setInputRate(vk::VertexInputRate::eVertex);
    return inputBindingDescriptions;
}

template<>
std::vector<vk::VertexInputAttributeDescription> MeshUtils::getVertexAttributeDescriptions<Vertex>() {
    std::vector<vk::VertexInputAttributeDescription> attribDescriptions;
    attribDescriptions.resize(4);

    // Position
    attribDescriptions[0].setBinding(0);
    attribDescriptions[0].setLocation(0);
    attribDescriptions[0].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
    attribDescriptions[0].setOffset(offsetof(Vertex, position));

    // Normal
    attribDescriptions[1].setBinding(0);
    attribDescriptions[1].setLocation(1);
    attribDescriptions[1].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
    attribDescriptions[1].setOffset(offsetof(Vertex, normal));

    // Tangent
    attribDescriptions[2].setBinding(0);
    attribDescriptions[2].setLocation(2);
    attribDescriptions[2].setFormat(vk::Format::eR32G32B32Sfloat); // vec3
    attribDescriptions[2].setOffset(offsetof(Vertex, tangent));

    // Texture
    attribDescriptions[3].setBinding(0);
    attribDescriptions[3].setLocation(3);
    attribDescriptions[3].setFormat(vk::Format::eR32G32Sfloat); // vec2
    attribDescriptions[3].setOffset(offsetof(Vertex, texture));
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
        LOG_FATAL("Get triangle vertex: internal triangle index %llu is out of range [0..3]", (uint64_t)index);
		assert(false);
	}
#endif

    Index vertexIndex = indices[index];

#if _DEBUG
    if (vertexIndex < 0 || vertexIndex >= vertices.size()) {
        LOG_FATAL("Triangle vertex index %llu is out of range [0..%llu]", (uint64_t)vertexIndex, (uint64_t)vertices.size());
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
        LOG_FATAL("Get triangle vertex: internal triangle index %llu is out of range [0..3]", (uint64_t)index);
		assert(false);
	}
#endif

    Index vertexIndex = indices[index];

#if _DEBUG
    if (vertexIndex < 0 || vertexIndex >= vertices.size()) {
        LOG_FATAL("Triangle vertex index %llu is out of range [0..%llu]", (uint64_t)vertexIndex, (uint64_t)vertices.size());
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
MeshData<Vertex_t>::MeshData(MeshPrimitiveType primitiveType) :
        m_currentTransform(1.0F),
        m_currentState(),
        m_primitiveType(primitiveType) {
}

template<typename Vertex_t>
MeshData<Vertex_t>::~MeshData() = default;

template<typename Vertex_t>
void MeshData<Vertex_t>::pushTransform() {
    m_transformStack.push(m_currentTransform);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::popTransform() {
#if _DEBUG
    if (m_transformStack.empty()) {
        LOG_FATAL("MeshData::popTransform(): Stack underflow");
		assert(false);
		return;
	}
#endif
    m_currentTransform = m_transformStack.top();
    m_transformStack.pop();
}

template<typename Vertex_t>
void MeshData<Vertex_t>::translate(const glm::vec3& v) {
    m_currentTransform = glm::translate(m_currentTransform, v);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::translate(float x, float y, float z) {
    translate(glm::vec3(x, y, z));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::rotate(const glm::quat& q) {
    m_currentTransform = glm::mat4_cast(q) * m_currentTransform;
}

template<typename Vertex_t>
void MeshData<Vertex_t>::rotate(float angle, const glm::vec3& axis) {
    m_currentTransform = glm::rotate(m_currentTransform, angle, axis);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::rotateDegrees(float angle, const glm::vec3& axis) {
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
void MeshData<Vertex_t>::scale(const glm::vec3& s) {
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
    for (size_t i = firstVertex; i < m_vertices.size(); ++i) {
        m_vertices[i] *= m_currentTransform;
    }
}

template<typename Vertex_t>
void MeshData<Vertex_t>::pushState() {
    m_stateStack.push(m_currentState);
    m_currentState.baseVertex = (Index)m_vertices.size();
    m_currentState.baseIndex = (Index)m_indices.size();
}

template<typename Vertex_t>
void MeshData<Vertex_t>::popState() {
#if _DEBUG
    if (m_stateStack.empty()) {
        LOG_FATAL("MeshData::popState(): Stack underflow");
		assert(false);
		return;
	}
#endif
    m_currentState = m_stateStack.top();
    m_stateStack.pop();
}

template<typename Vertex_t>
void MeshData<Vertex_t>::reset(MeshPrimitiveType primitiveType) {
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
void MeshData<Vertex_t>::createTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2, const glm::vec3& normal) {
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
void MeshData<Vertex_t>::createQuad(const Vertex& v00, const Vertex& v01, const Vertex& v11, const Vertex& v10) {
    addQuad(v00, v01, v11, v10);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createQuad(const glm::vec3& p00, const glm::vec3& p01, const glm::vec3& p11, const glm::vec3& p10, const glm::vec3& n00, const glm::vec3& n01, const glm::vec3& n11, const glm::vec3& n10, const glm::vec2& t00, const glm::vec2& t01, const glm::vec2& t11, const glm::vec2& t10) {
    createQuad(Vertex(p00, n00, t00), Vertex(p01, n01, t01), Vertex(p11, n11, t11), Vertex(p10, n10, t10));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createQuad(const glm::vec3& p00, const glm::vec3& p01, const glm::vec3& p11, const glm::vec3& p10, const glm::vec3& normal, const glm::vec2& t00, const glm::vec2& t01, const glm::vec2& t11, const glm::vec2& t10) {
    createQuad(p00, p01, p11, p10, normal, normal, normal, normal, t00, t01, t11, t10);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createQuad(const glm::vec3& p00, const glm::vec3& p01, const glm::vec3& p11, const glm::vec3& p10, const glm::vec3& normal) {
    createQuad(p00, p01, p11, p10, normal, normal, normal, normal, glm::vec2(0.0F, 0.0F), glm::vec2(0.0F, 1.0F), glm::vec2(1.0F, 1.0F), glm::vec2(1.0F, 0.0F));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createPlane(const glm::vec3& origin, const glm::vec3& u, const glm::vec3& v, const glm::vec3& normal, const glm::vec2& scale, const glm::uvec2& subdivisions) {
    uint32_t vertexCountU = subdivisions.x + 2;
    uint32_t vertexCountV = subdivisions.y + 2;

    float textureIncrementU = 1.0F / (float)(vertexCountU - 1);
    float textureIncrementV = 1.0F / (float)(vertexCountV - 1);
    float offsetIncrementU = scale.x * textureIncrementU;
    float offsetIncrementV = scale.y * textureIncrementV;

    bool reversedNormal = glm::dot(normal, glm::cross(u, v)) < 0.0F;

    Index iu, iv;
    float offsetU, offsetV;
    glm::vec2 texture;

    pushState();

    for (iv = 0, offsetV = 0.0F, texture.y = 0.0F; iv < (Index)vertexCountV; ++iv) {
        for (iu = 0, offsetU = 0.0F, texture.x = 0.0F; iu < (Index)vertexCountU; ++iu) {

            glm::vec3 position = origin + (u * offsetU) + (v * offsetV);
            addVertex(position, normal, texture);

            offsetU += offsetIncrementU;
            texture.x += textureIncrementU;
        }
        offsetV += offsetIncrementV;
        texture.y += textureIncrementV;
    }


    for (iv = 0; iv < (Index)(vertexCountV - 1); ++iv) {
        for (iu = 0; iu < (Index)(vertexCountU - 1); ++iu) {
            Index i00 = (iu + 0) + (iv + 0) * vertexCountV;
            Index i10 = (iu + 1) + (iv + 0) * vertexCountV;
            Index i11 = (iu + 1) + (iv + 1) * vertexCountV;
            Index i01 = (iu + 0) + (iv + 1) * vertexCountV;

            if (reversedNormal) {
                addQuad(i00, i01, i11, i10);
            } else {
                addQuad(i00, i10, i11, i01);
            }
        }
    }

    popState();
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createDisc(const glm::vec3& center, const glm::vec3& u, const glm::vec3& v, const glm::vec3& normal, float radius, uint32_t segments) {
    if (radius < 1e-6)
        return;

    segments = glm::max(segments, 3u);

    bool reversedNormal = glm::dot(normal, glm::cross(u, v)) < 0.0F;

    pushState();

    float theta = 0.0;
    float thetaIncrement = glm::two_pi<float>() / (float)segments;

    glm::vec2 uv;

    for (uint32_t i = 0; i < segments; ++i) {
        uv.x = glm::cos(theta);
        uv.y = glm::sin(theta);

        glm::vec3 position = center + (u * uv.x * radius) + (v * uv.y * radius);
        addVertex(position, normal, uv * 0.5F + 0.5F);

        theta += thetaIncrement;
    }

    Index i0 = m_primitiveType != PrimitiveType_Line ? addVertex(center, normal, glm::vec2(0.5, 0.5)) : 0;

    for (uint32_t i = 0; i < segments; ++i) {
        if (m_primitiveType == PrimitiveType_Line) {
            createLinePrimitive(i, (i + 1) % segments);
        } else if (m_primitiveType == PrimitiveType_Point) {
            createPointPrimitive(i);
        } else {
            addTriangle(i0, i, (i + 1) % segments);
        }
    }

    popState();
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createCuboid(const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& tex0, const glm::vec3& tex1) {
    createQuad(
            glm::vec3(pos0.x, pos0.y, pos0.z),
            glm::vec3(pos0.x, pos0.y, pos1.z),
            glm::vec3(pos0.x, pos1.y, pos1.z),
            glm::vec3(pos0.x, pos1.y, pos0.z),
            glm::vec3(-1.0F, 0.0F, 0.0F),
            glm::vec2(tex0.x, tex1.y),  // 01
            glm::vec2(tex1.x, tex1.y),  // 11
            glm::vec2(tex1.x, tex0.y),  // 10
            glm::vec2(tex0.x, tex0.y)   // 00
    );
    createQuad(
            glm::vec3(pos1.x, pos0.y, pos1.z),
            glm::vec3(pos1.x, pos0.y, pos0.z),
            glm::vec3(pos1.x, pos1.y, pos0.z),
            glm::vec3(pos1.x, pos1.y, pos1.z),
            glm::vec3(+1.0F, 0.0F, 0.0F),
            glm::vec2(tex0.x, tex1.y), // 01
            glm::vec2(tex1.x, tex1.y), // 11
            glm::vec2(tex1.x, tex0.y), // 10
            glm::vec2(tex0.x, tex0.y)  // 00
    );
    createQuad(
            glm::vec3(pos1.x, pos0.y, pos0.z),
            glm::vec3(pos1.x, pos0.y, pos1.z),
            glm::vec3(pos0.x, pos0.y, pos1.z),
            glm::vec3(pos0.x, pos0.y, pos0.z),
            glm::vec3(0.0F, -1.0F, 0.0F),
            glm::vec2(tex1.x, tex1.y), // 11
            glm::vec2(tex1.x, tex0.y), // 10
            glm::vec2(tex0.x, tex0.y), // 00
            glm::vec2(tex0.x, tex1.y)  // 01
    );
    createQuad(
            glm::vec3(pos0.x, pos1.y, pos0.z),
            glm::vec3(pos0.x, pos1.y, pos1.z),
            glm::vec3(pos1.x, pos1.y, pos1.z),
            glm::vec3(pos1.x, pos1.y, pos0.z),
            glm::vec3(0.0F, +1.0F, 0.0F),
            glm::vec2(tex0.x, tex0.y), // 00
            glm::vec2(tex0.x, tex1.y), // 01
            glm::vec2(tex1.x, tex1.y), // 11
            glm::vec2(tex1.x, tex0.y)  // 10
    );
    createQuad(
            glm::vec3(pos0.x, pos1.y, pos0.z),
            glm::vec3(pos1.x, pos1.y, pos0.z),
            glm::vec3(pos1.x, pos0.y, pos0.z),
            glm::vec3(pos0.x, pos0.y, pos0.z),
            glm::vec3(0.0F, 0.0F, -1.0F),
            glm::vec2(tex1.x, tex0.y), // 10
            glm::vec2(tex0.x, tex0.y), // 00
            glm::vec2(tex0.x, tex1.y), // 01
            glm::vec2(tex1.x, tex1.y)  // 11
    );
    createQuad(
            glm::vec3(pos1.x, pos1.y, pos1.z),
            glm::vec3(pos0.x, pos1.y, pos1.z),
            glm::vec3(pos0.x, pos0.y, pos1.z),
            glm::vec3(pos1.x, pos0.y, pos1.z),
            glm::vec3(0.0F, 0.0F, +1.0F),
            glm::vec2(tex1.x, tex0.y), // 10
            glm::vec2(tex0.x, tex0.y), // 00
            glm::vec2(tex0.x, tex1.y), // 01
            glm::vec2(tex1.x, tex1.y)  // 11
    );
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createCuboid(const glm::vec3& pos0, const glm::vec3& pos1) {
    createCuboid(pos0, pos1, glm::vec3(0.0F), glm::vec3(1.0F));
}

template<typename Vertex_t>
void MeshData<Vertex_t>::createUVSphere(const glm::vec3& center, float radius, uint32_t slices, uint32_t stacks) {
    pushState();

    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 tex;
    float fstacks = (float)stacks;
    float fslices = (float)slices;
    for (size_t i = 0; i <= stacks; ++i) {
        tex.y = (float)(i) / fstacks;
        float phi = glm::pi<float>() * (tex.y - 0.5F); // -90 to +90 degrees
        norm.y = glm::sin(phi);
        pos.y = center.y + norm.y * radius;

        for (size_t j = 0; j <= slices; ++j) {
            tex.x = (float)(j) / fslices;
            float theta = glm::two_pi<float>() * tex.x; // 0 to 360 degrees

            norm.x = glm::cos(phi) * glm::sin(theta);
            norm.z = glm::cos(phi) * glm::cos(theta);
            pos.x = center.x + norm.x * radius;
            pos.z = center.z + norm.z * radius;

            addVertex(pos, norm, tex);

        }
    }

    for (size_t i0 = 0; i0 < stacks; ++i0) {
        size_t i1 = (i0 + 1);

        for (size_t j0 = 0; j0 < slices; ++j0) {
            size_t j1 = (j0 + 1);

            Index i00 = (Index)(i0 * (slices + 1) + j0);
            Index i10 = (Index)(i0 * (slices + 1) + j1);
            Index i01 = (Index)(i1 * (slices + 1) + j0);
            Index i11 = (Index)(i1 * (slices + 1) + j1);

            addQuad(i00, i10, i11, i01);
        }
    }

    //for (size_t i = 0; i <= stacks; ++i) {
    //	for (size_t j = 0; j <= slices; ++j) {
    //		if (i > 0) {
    //			size_t j1 = (j + 1) % slices;
    //			size_t i0 = (i - 1);
    //			Index i00 = (Index)(i0 * (slices + 1) + j);
    //			Index i10 = (Index)(i0 * (slices + 1) + j1);
    //			Index i01 = (Index)(i * (slices + 1) + j);
    //			Index i11 = (Index)(i * (slices + 1) + j1);
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
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::addVertex(float px, float py, float pz, float nx, float ny, float nz, float tu, float tv) {
    return addVertex(Vertex(px, py, pz, nx, ny, nz, tu, tv));
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
        default:
            assert(false); // Unsupported primitive type
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
        default:
            assert(false); // Unsupported primitive type
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
    glm::dvec3 bucketCenter(0.0F);

    const size_t firstIndex = currentStateOnly ? m_currentState.baseIndex : 0;

    for (size_t i = firstIndex; i < m_indices.size(); ++i) {
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
    glm::vec3 minExtent(+INFINITY);
    glm::vec3 maxExtent(-INFINITY);

    const size_t firstPrimitive = currentStateOnly ? m_currentState.baseIndex : 0;

    for (size_t i = firstPrimitive; i < m_indices.size(); ++i) {
        for (size_t j = 0; j < 3; ++j) {
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
MeshPrimitiveType MeshData<Vertex_t>::getPrimitiveType() const {
    return m_primitiveType;
}

template<typename Vertex_t>
void MeshData<Vertex_t>::computeTangents() {
    MeshData<Vertex_t>::computeTangents(m_vertices, m_indices, m_primitiveType);
}

template<typename Vertex_t>
void MeshData<Vertex_t>::computeTangents(std::vector<Vertex_t>& vertices, const std::vector<uint32_t>& indices, MeshPrimitiveType primitiveType) {
    using index_t = MeshData<Vertex_t>::Index;

    if (primitiveType != MeshPrimitiveType::PrimitiveType_Triangle) {
        LOG_ERROR("Unable to compute mesh tangents. Mesh primitive type must be triangles");
        return;
    }

    if (indices.size() % 3 != 0) {
        LOG_ERROR("Unable to compute mesh tangents. Triangle mesh indices must have a size of a multiple of 3");
        return;
    }

    std::vector<glm::vec3> tangents;
    tangents.resize(indices.size());

    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        Vertex_t& v0 = vertices[i0];
        Vertex_t& v1 = vertices[i1];
        Vertex_t& v2 = vertices[i2];

        glm::vec3 e0 = v1.position - v0.position;
        glm::vec3 e1 = v2.position - v0.position;

        glm::vec2 dUV0 = v1.texture - v0.texture;
        glm::vec2 dUV1 = v2.texture - v0.texture;
        float r = (dUV0.x * dUV1.y) - (dUV0.y * dUV1.x);

        if (abs(r) > 1e-9) {
            r = 1.0F / r;
            glm::vec3 tangent = (e0 * dUV1.y - e1 * dUV0.y) * r;
//            tangent = normalize(tangent);
            tangents[i + 0] = tangent;
            tangents[i + 1] = tangent;
            tangents[i + 2] = tangent;
        }
    }

    for (size_t i = 0; i < indices.size(); ++i)
        vertices[indices[i]].tangent += tangents[i];

    for (size_t i = 0; i < indices.size(); ++i)
        vertices[indices[i]].tangent = glm::normalize(vertices[indices[i]].tangent);
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::createTrianglePrimitive(Index i0, Index i1, Index i2) {
    assert(m_primitiveType == PrimitiveType_Triangle);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    m_indices.emplace_back(i1 + m_currentState.baseVertex);
    m_indices.emplace_back(i2 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::createLinePrimitive(Index i0, Index i1) {
    assert(m_primitiveType == PrimitiveType_Line);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    m_indices.emplace_back(i1 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}

template<typename Vertex_t>
typename MeshData<Vertex_t>::Index MeshData<Vertex_t>::createPointPrimitive(Index i0) {
    assert(m_primitiveType == PrimitiveType_Point);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}

#endif //WORLDENGINE_MESHDATA_H
