#include "core/engine/geometry/MeshData.h"
#include <fstream>
#include <filesystem>

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

MeshData::Vertex::Vertex() :
        position(0.0F),
        normal(0.0F),
        texture(0.0F) {
}

MeshData::Vertex::Vertex(const Vertex& copy) :
        position(copy.position),
        normal(copy.normal),
        texture(copy.texture) {
}

MeshData::Vertex::Vertex(glm::vec3 position, glm::vec3 normal, glm::vec2 texture) :
        position(position),
        normal(normal),
        texture(texture) {
}

MeshData::Vertex::Vertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty) :
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

MeshData::Triangle::Triangle() :
        i0(0), i1(0), i2(0) {
}

MeshData::Triangle::Triangle(Index i0, Index i1, Index i2) :
        i0(i0), i1(i1), i2(i2) {
}

MeshData::Vertex& MeshData::Triangle::getVertex(std::vector<Vertex>& vertices, Index index) const {
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

MeshData::Vertex& MeshData::Triangle::getVertex(MeshData& meshData, Index index) const {
    return getVertex(meshData.m_vertices, index);
}

const MeshData::Vertex& MeshData::Triangle::getVertex(const std::vector<MeshData::Vertex>& vertices, MeshData::Index index) const {
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

const MeshData::Vertex& MeshData::Triangle::getVertex(const MeshData& meshData, MeshData::Index index) const {
    return getVertex(meshData.m_vertices, index);
}

glm::vec3 MeshData::Triangle::getFacing(const std::vector<Vertex>& vertices) const {
    const glm::vec3& v0 = getVertex(vertices, 0).position;
    const glm::vec3& v1 = getVertex(vertices, 1).position;
    const glm::vec3& v2 = getVertex(vertices, 2).position;

    return glm::cross(v1 - v0, v2 - v0);
}

glm::vec3 MeshData::Triangle::getFacing(const MeshData& meshData) const {
    return getFacing(meshData.m_vertices);
}

glm::vec3 MeshData::Triangle::getNormal(const std::vector<Vertex>& vertices) const {
    return glm::normalize(getFacing(vertices));
}

glm::vec3 MeshData::Triangle::getNormal(const MeshData& meshData) const {
    return getNormal(meshData.m_vertices);
}





MeshData::MeshData():
    MeshData(PrimitiveType_Line) {
}

MeshData::MeshData(const MeshPrimitiveType& primitiveType) :
    m_currentTransform(1.0F),
    m_currentState(),
    m_primitiveType(primitiveType) {
}

MeshData::~MeshData() {
}

void MeshData::pushTransform() {
    m_transformStack.push(m_currentTransform);
}

void MeshData::popTransform() {
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

void MeshData::applyTransform(bool currentStateOnly) {
    size_t firstVertex = currentStateOnly ? m_currentState.baseVertex : 0;
    for (int i = firstVertex; i < m_vertices.size(); ++i) {
        m_vertices[i] *= m_currentTransform;
    }
}

void MeshData::pushState() {
    m_stateStack.push(m_currentState);
    m_currentState.baseVertex = m_vertices.size();
    m_currentState.baseIndex = m_indices.size();
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

void MeshData::reset(const MeshPrimitiveType &primitiveType) {
    clear();
    m_primitiveType = primitiveType;
}

void MeshData::clear() {
    m_currentTransform = glm::mat4(1.0F);
    m_currentState = State();

    while (!m_stateStack.empty())
        m_stateStack.pop();

    while (!m_transformStack.empty())
        m_transformStack.pop();

    m_vertices.clear();
    m_indices.clear();
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
    addQuad(v0, v1, v2, v3);
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

void MeshData::addTriangle(Index i0, Index i1, Index i2) {
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

void MeshData::addTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    Index i0 = addVertex(v0);
    Index i1 = addVertex(v1);
    Index i2 = addVertex(v2);
    addTriangle(i0, i1, i2);
}

void MeshData::addQuad(Index i0, Index i1, Index i2, Index i3) {
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

void MeshData::addQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    Index i0 = addVertex(v0);
    Index i1 = addVertex(v1);
    Index i2 = addVertex(v2);
    Index i3 = addVertex(v3);
    addQuad(i0, i1, i2, i3);
}

const std::vector<MeshData::Vertex>& MeshData::getVertices() const {
    return m_vertices;
}

const std::vector<MeshData::Index>& MeshData::getIndices() const {
    return m_indices;
}

std::vector<MeshData::Vertex> &MeshData::vertices() {
    return m_vertices;
}

std::vector<MeshData::Index> &MeshData::indices() {
    return m_indices;
}

glm::vec3 MeshData::calculateCenterOffset(bool currentStateOnly) const {
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

glm::mat4 MeshData::calculateBoundingBox(bool currentStateOnly) const {
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

size_t MeshData::getVertexCount() const {
    return m_vertices.size();
}

size_t MeshData::getIndexCount() const {
    return m_indices.size();
}

size_t MeshData::getPolygonCount() const {
    return MeshData::getPolygonCount(m_indices.size(), m_primitiveType);
}

size_t MeshData::getPolygonCount(const size_t& numIndices, const MeshPrimitiveType& primitiveType) {
    switch (primitiveType) {
        case PrimitiveType_Point: return numIndices;
        case PrimitiveType_Line: return numIndices / 2;
        case PrimitiveType_Triangle: return numIndices / 3;
        default:
            assert(false); // Invalid primitive type
            return 0;
    }
}

const MeshPrimitiveType& MeshData::getPrimitiveType() const {
    return m_primitiveType;
}

MeshData::Index MeshData::createTrianglePrimitive(const Index& i0, const Index& i1, const Index& i2) {
    assert(m_primitiveType == PrimitiveType_Triangle);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    m_indices.emplace_back(i1 + m_currentState.baseVertex);
    m_indices.emplace_back(i2 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}

MeshData::Index MeshData::createLinePrimitive(const Index& i0, const Index& i1) {
    assert(m_primitiveType == PrimitiveType_Line);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    m_indices.emplace_back(i1 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}

MeshData::Index MeshData::createPointPrimitive(const Index& i0) {
    assert(m_primitiveType == PrimitiveType_Point);
    size_t index = m_indices.size() - m_currentState.baseIndex;
    m_indices.emplace_back(i0 + m_currentState.baseVertex);
    return static_cast<Index>(index);
}


inline std::string trim(const std::string& str, const std::string& whitespace = " \t") {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

inline std::vector<std::string> split(const std::string& string, const char delimiter, bool allowEmpty = true, bool doTrim = true) {
    std::vector<std::string> comps;

    size_t start = 0;
    size_t end = string.find_first_of(delimiter);

    while (true) {
        std::string comp = string.substr(start, end - start);

        if (doTrim)
            comp = trim(comp);

        if (allowEmpty || !comp.empty())
            comps.emplace_back(comp);

        if (end == std::string::npos)
            break;

        start = end + 1;
        end = string.find_first_of(delimiter, start);
    }

    return comps;
}





struct index {
    union {
        struct { uint32_t p, t, n; };
        glm::uvec3 k;
        uint32_t i[3];
    };
};

struct face {
    union {
        index v[3];
    };
};

struct object {
    std::string objectName;
    std::string groupName;
    std::string materialName;
    uint32_t triangleBeginIndex;
    uint32_t triangleEndIndex;
};



void compileOBJObject(
        object& currentObject,
        std::vector<MeshData::Vertex>& vertices,
        std::vector<MeshData::Triangle>& triangles,
        std::vector<face>& faces,
        std::vector<glm::vec3>& positions,
        std::vector<glm::vec2>& textures,
        std::vector<glm::vec3>& normals,
        std::unordered_map<glm::uvec3, uint32_t>& mappedIndices) {

    constexpr uint32_t npos = uint32_t(-1);

    currentObject.triangleBeginIndex = triangles.size();

    for (int i = 0; i < faces.size(); ++i) {
        const face& face = faces[i];
        MeshData::Triangle tri;

        bool useFaceNormal = false;

        for (int j = 0; j < 3; ++j) {
            const index& index = face.v[j];

            uint32_t mappedIndex = mappedIndices[index.k];

            if (mappedIndex == npos) {
                glm::vec3 position = index.p != npos ? positions[index.p] : glm::vec3(0.0F);
                glm::vec3 normal = index.n != npos ? normals[index.n] : glm::vec3(NAN);
                glm::vec2 texture = index.t != npos ? textures[index.t] : glm::vec2(0.0F);
                mappedIndex = vertices.size();
                mappedIndices[index.k] = mappedIndex;
                vertices.emplace_back(position, normal, texture);
            }

            tri.indices[j] = mappedIndex;
        }

        MeshData::Vertex& v0 = tri.getVertex(vertices, 0);
        MeshData::Vertex& v1 = tri.getVertex(vertices, 1);
        MeshData::Vertex& v2 = tri.getVertex(vertices, 2);

        // TODO: average face normals if multiple faces share a vertex
        if (std::isnan(v0.normal.x) || std::isnan(v1.normal.x) || std::isnan(v2.normal.x)) {
            glm::vec3 faceNormal = tri.getNormal(vertices);
            v0.normal = faceNormal;
            v1.normal = faceNormal;
            v2.normal = faceNormal;
        }

        triangles.emplace_back(tri);
    }

    currentObject.triangleEndIndex = triangles.size();
}

bool MeshLoader::loadOBJFile(const std::string& filePath, MeshData& meshData) {
    // TODO: this could be optimised a lot.

    printf("Loading OBJ file \"%s\"\n", filePath.c_str());
    std::ifstream stream(filePath.c_str(), std::ifstream::in);

    if (!stream.is_open()) {
        printf("Failed to open OBJ file\n");
        return false;
    }

    constexpr uint32_t npos = uint32_t(-1);


    uint32_t nextMaterialId = 0;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> textures;
    std::vector<glm::vec3> normals;
    std::vector<face> faces;

    std::vector<MeshData::Vertex> vertices;
    std::vector<MeshData::Triangle> triangles;
    std::unordered_map<glm::uvec3, uint32_t> mappedIndices;
    //std::unordered_map<std::string, MaterialSet*> loadedMaterialSets;

    std::string currentObjectName = "default";
    std::string currentGroupName = "default";
    std::string currentMaterialName = "default";

    object currentObject = { currentObjectName, currentGroupName, currentMaterialName };
    std::vector<object> objects;

    uint32_t lineNumber = 0;
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        lineNumber++;

        if (line.empty()) {
            continue;
        }

        try {
            if (line.rfind("v ", 0) == 0) { // line is a vertex position
                std::vector<std::string> comps = split(line, ' ', false);
                float x = std::stof(comps[1]);
                float y = std::stof(comps[2]);
                float z = std::stof(comps[3]);
                positions.emplace_back(x, y, z);

            } else if (line.rfind("vt ", 0) == 0) { // line is a vertex texture
                std::vector<std::string> comps = split(line, ' ', false);
                float x = std::stof(comps[1]);
                float y = std::stof(comps[2]);
                textures.emplace_back(x, y);

            } else if (line.rfind("vn ", 0) == 0) { // line is a vertex normal
                std::vector<std::string> comps = split(line, ' ', false);
                float x = std::stof(comps[1]);
                float y = std::stof(comps[2]);
                float z = std::stof(comps[3]);
                normals.emplace_back(x, y, z);

            } else if (line.rfind("f ", 0) == 0) { // line is a face definition
                std::vector<std::string> faceComps = split(line, ' ');

                uint32_t faceSize = faceComps.size() - 1;

                if (faceSize < 3) {
                    printf("Warning: Loading OBJ file \"%s\", skipping invalid face on line %d\n", filePath.c_str(), lineNumber);
                    continue;
                }

                index* indices = new index[faceSize];
                for (int i = 0; i < faceSize; i++) {
                    std::vector<std::string> vertComps = split(faceComps[i + 1], '/');

                    indices[i].p = npos;
                    indices[i].t = npos;
                    indices[i].n = npos;
                    if (vertComps.size() == 3) { // p/t/n | p//n
                        indices[i].p = std::stoi(vertComps[0]) - 1;
                        indices[i].n = std::stoi(vertComps[2]) - 1;
                        if (!vertComps[1].empty())
                            indices[i].t = std::stoi(vertComps[1]) - 1;

                    } else if (vertComps.size() == 2) { // p/t
                        indices[i].p = std::stoi(vertComps[0]) - 1;
                        indices[i].t = std::stoi(vertComps[1]) - 1;

                    } else if (vertComps.size() == 1) { // p
                        indices[i].p = std::stoi(vertComps[0]) - 1;

                    } else {
                        throw Exception("Invalid or unsupported face vertex definition format");
                    }

                    if (indices[i].p == npos) { // position reference is required
                        throw Exception("Invalid or missing face vertex position index");
                    }

                    mappedIndices[indices[i].k] = npos; // initialize to invalid reference
                }

                // triangle fan
                for (int i = 1; i < faceSize - 1; i++) {
                    face f;
                    f.v[0] = indices[0];
                    f.v[1] = indices[i];
                    f.v[2] = indices[i + 1];
                    faces.push_back(f);
                }

                //if (faceSize >= 3) {
                //	face f;
                //	f.v0 = indices[0];
                //	f.v1 = indices[1];
                //	f.v2 = indices[2];
                //	faces.push_back(f);
                //
                //	if (faceSize >= 4) {
                //		f.v0 = indices[0];
                //		f.v1 = indices[2];
                //		f.v2 = indices[3];
                //		faces.push_back(f);
                //
                //		if (faceSize >= 5) {
                //			printf("[%d] INVALID FACE - %d VERTICES\n", lineNumber, faceSize);
                //		}
                //	}
                //}

                delete[] indices;
            } else if (line.rfind("o ", 0) == 0) { // line is an object definition
                currentObjectName = line.substr(2);

                if (!faces.empty()) {
                    compileOBJObject(currentObject, vertices, triangles, faces, positions, textures, normals, mappedIndices);
                    objects.push_back(currentObject);
                }

                currentObject = { currentObjectName, currentGroupName, currentMaterialName };
                mappedIndices.clear(); // Clear mapped indices - we don't want shared vertices across mesh boundries.
                faces.clear();
            } else if (line.rfind("g ", 0) == 0) { // line is a group definition
                currentGroupName = line.substr(2);

                if (!faces.empty()) {
                    compileOBJObject(currentObject, vertices, triangles, faces, positions, textures, normals, mappedIndices);
                    objects.push_back(currentObject);
                }

                currentObject = { currentObjectName, currentGroupName, currentMaterialName };
                faces.clear();
            }
            else if (line.rfind("usemtl ", 0) == 0) { // Line defines material to use for all folowing polygons
                std::string materialName = line.substr(7);

                if (materialName.empty())
                    throw Exception("Using material with invalid name");

                if (currentMaterialName != materialName) {
                    currentMaterialName = materialName;

                    if (!faces.empty()) {
                        compileOBJObject(currentObject, vertices, triangles, faces, positions, textures, normals, mappedIndices);
                        objects.push_back(currentObject);
                    }

                    currentObject = { currentObjectName, currentGroupName, currentMaterialName };
                    //mappedIndices.clear(); // Clear mapped indices - we don't want shared vertices across material boundries.
                    faces.clear();
                }
            }
            //else if (line.rfind("mtllib ", 0) == 0) { // Line is a material loading command
            //	std::vector<std::string> materialPaths = split(line, ' ');
            //
            //	for (int i = 1; i < materialPaths.size(); i++) {
            //		auto it = loadedMaterialSets.find(materialPaths[i]);
            //
            //		if (it != loadedMaterialSets.end()) { // already loaded, don't re-load it.
            //			continue;
            //		}
            //
            //		std::string mtlPath = mtlDir + "/" + materialPaths[i];
            //		MaterialSet* mtl = OBJ::readMTL(mtlPath);
            //		if (mtl == NULL) {
            //			warn("Failed to read or parse MTL file \"%s\"\n", materialPaths[i].c_str());
            //			continue;
            //		}
            //		obj->m_materialSets.push_back(mtl);
            //		loadedMaterialSets[materialPaths[i]] = mtl;
            //	}
            //}
        } catch (const std::exception& e) {
            printf("Error while parsing OBJ line \"%s\" - %s\n", line.c_str(), e.what());
            return false;
        }
    }

    if (!faces.empty()) {
        compileOBJObject(currentObject, vertices, triangles, faces, positions, textures, normals, mappedIndices);
        objects.push_back(currentObject);
    }

    //OBJ::calculateTangents(vertices, triangles);
    //
    //obj->m_objects.swap(objects);
    //obj->m_vertices.swap(vertices);
    //obj->m_triangles.swap(triangles);

    for (int i = 0; i < vertices.size(); ++i) {
        meshData.addVertex(vertices[i]);
    }

    for (int i = 0; i < triangles.size(); ++i) {
        meshData.addTriangle(triangles[i].i0, triangles[i].i1, triangles[i].i2);
    }

    return true;
}

// Version must be incremented whenever the read/write methods are changed,
// otherwise garbage data will get read from older versions of the cache file.
#define MESH_CACHE_FILE_VERSION 1

bool readMeshCache(const std::filesystem::path& path, MeshData& meshData) {

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        printf("Failed to open cached mesh file \"%s\"\n", path.c_str());
        return false;
    }

    uint64_t version;
    file.read((char*)(&version), 8);
    if (version != MESH_CACHE_FILE_VERSION)
        return false; // Incompatible file version. Cache file should be re-generated

    uint64_t vertexCount = 0;
    uint64_t indexCount = 0;
    uint32_t primType;
    file.read((char*)(&vertexCount), 8);
    file.read((char*)(&indexCount), 8);
    file.read((char*)(&primType), 4);
    meshData.reset((MeshPrimitiveType)primType);
    meshData.vertices().resize(vertexCount);
    meshData.indices().resize(indexCount);
    file.read((char*)(meshData.vertices().data()), vertexCount * sizeof(MeshData::Vertex));
    file.read((char*)(meshData.indices().data()), indexCount * sizeof(MeshData::Index));
    return true;
}

bool writeMeshCache(const std::filesystem::path& path, MeshData& meshData) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        printf("Failed to create mesh cache file \"%s\"\n", path.c_str());
        return false;
    }

    printf("Writing mesh cache file \"%s\"\n", path.c_str());

    uint64_t vertexCount = meshData.vertices().size();
    uint64_t indexCount = meshData.indices().size();
    uint32_t primType = meshData.getPrimitiveType();

    uint64_t version = MESH_CACHE_FILE_VERSION;
    file.write((char*)(&version), 8);

    file.write((char*)(&vertexCount), 8);
    file.write((char*)(&indexCount), 8);
    file.write((char*)(&primType), 4);
    file.write((char*)(meshData.vertices().data()), vertexCount * sizeof(MeshData::Vertex));
    file.write((char*)(meshData.indices().data()), indexCount * sizeof(MeshData::Index));
    return true;
}

bool MeshLoader::loadMeshData(const std::string& filePath, MeshData &meshData) {
    size_t extensionPos = filePath.find_last_of('.');

//    std::string sourceExtension;
//    if (extensionPos != std::string::npos)
//        sourceExtension = filePath.substr(extensionPos + 1);

    std::filesystem::path sourceMeshFilePath(filePath);
    std::filesystem::path cachedMeshFilePath(filePath.substr(0, extensionPos) + ".mesh");

    if (std::filesystem::exists(cachedMeshFilePath)) {

        if (std::filesystem::exists(sourceMeshFilePath)) {
            auto sourceMeshTimestamp = std::filesystem::last_write_time(sourceMeshFilePath);
            auto cachedMeshTimestamp = std::filesystem::last_write_time(cachedMeshFilePath);
            if (cachedMeshTimestamp < sourceMeshTimestamp) {
                // Source was modified since it was last cached.
                if (!loadOBJFile(sourceMeshFilePath.string(), meshData))
                    return false;
                writeMeshCache(cachedMeshFilePath, meshData);
                return true;
            }
        }

        if (readMeshCache(cachedMeshFilePath, meshData))
            return true;
        // If reading the cache file failed, it will get re-generated.
    }

    if (!loadOBJFile(sourceMeshFilePath.string(), meshData))
        return false;
    writeMeshCache(cachedMeshFilePath, meshData);
    return true;
}
