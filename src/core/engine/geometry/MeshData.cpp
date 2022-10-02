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

Vertex::Vertex() :
    position(0.0F),
    normal(0.0F),
    tangent(0.0F),
    texture(0.0F) {
}

Vertex::Vertex(const Vertex& copy) :
    position(copy.position),
    normal(copy.normal),
    tangent(copy.tangent),
    texture(copy.texture) {
}

Vertex::Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& texture):
    position(position),
    normal(normal),
    tangent(0.0F),
    texture(texture) {
}

Vertex::Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec3& tangent, const glm::vec2& texture):
    position(position),
    normal(normal),
    tangent(tangent),
    texture(texture) {
}

Vertex::Vertex(const float& px, const float& py, const float& pz, const float& nx, const float& ny, const float& nz, const float& tu, const float& tv) :
    position(px, py, pz),
    normal(nx, ny, nz),
    tangent(0.0F, 0.0F, 0.0F),
    texture(tu, tv) {
}

Vertex::Vertex(const float& px, const float& py, const float& pz, const float& nx, const float& ny, const float& nz, const float& tx, const float& ty, const float& tz, const float& tu, const float& tv) :
    position(px, py, pz),
    normal(nx, ny, nz),
    tangent(tx, ty, tz),
    texture(tu, tv) {
}

Vertex Vertex::operator*(const glm::mat4& m) const {
    Vertex v(*this);
    v *= m;
    return v;
}

Vertex& Vertex::operator*=(const glm::mat4& m) {
    glm::mat4 nm = glm::transpose(glm::inverse(m));
    position = glm::vec3(m * glm::vec4(position, 1.0F));
    normal = glm::vec3(nm * glm::vec4(normal, 0.0F));
    tangent = glm::vec3(nm * glm::vec4(tangent, 0.0F));
    return *this;
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





struct Index {
    union {
        struct { uint32_t p, t, n; };
        glm::uvec3 k;
        uint32_t i[3];
    };
};

struct Face {
    union {
        Index v[3];
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
        std::vector<MeshUtils::OBJMeshData::Vertex>& vertices,
        std::vector<MeshUtils::OBJMeshData::Triangle>& triangles,
        std::vector<Face>& faces,
        std::vector<glm::vec3>& positions,
        std::vector<glm::vec2>& textures,
        std::vector<glm::vec3>& normals,
        std::unordered_map<glm::uvec3, uint32_t>& mappedIndices) {

    constexpr uint32_t npos = uint32_t(-1);

    currentObject.triangleBeginIndex = triangles.size();

    size_t numPositions = positions.size();
    size_t numTriangles = triangles.size();

    for (int i = 0; i < faces.size(); ++i) {
        const Face& face = faces[i];
        MeshUtils::OBJMeshData::Triangle tri;

        bool useFaceNormal = false;

        for (int j = 0; j < 3; ++j) {
            const Index& index = face.v[j];

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

        MeshUtils::OBJMeshData::Vertex& v0 = tri.getVertex(vertices, 0);
        MeshUtils::OBJMeshData::Vertex& v1 = tri.getVertex(vertices, 1);
        MeshUtils::OBJMeshData::Vertex& v2 = tri.getVertex(vertices, 2);

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

bool MeshUtils::loadOBJFile(const std::string& filePath, MeshUtils::OBJMeshData& meshData) {
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
    std::vector<Face> faces;

    std::vector<MeshUtils::OBJMeshData::Vertex> vertices;
    std::vector<MeshUtils::OBJMeshData::Triangle> triangles;
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

                Index* indices = new Index[faceSize];
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
                    Face f;
                    f.v[0] = indices[0];
                    f.v[1] = indices[i];
                    f.v[2] = indices[i + 1];
                    faces.push_back(f);
                }

                //if (faceSize >= 3) {
                //	Face f;
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

    meshData.computeTangents();

    return true;
}

// Version must be incremented whenever the read/write methods or Vertex structure is changed,
// otherwise garbage data will get read from older versions of the cache file.
#define MESH_CACHE_FILE_VERSION 5

bool readMeshCache(const std::filesystem::path& path, MeshUtils::OBJMeshData& meshData) {

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
    file.read((char*)(meshData.vertices().data()), vertexCount * sizeof(MeshUtils::OBJMeshData::Vertex));
    file.read((char*)(meshData.indices().data()), indexCount * sizeof(MeshUtils::OBJMeshData::Index));
    return true;
}

bool writeMeshCache(const std::filesystem::path& path, MeshUtils::OBJMeshData& meshData) {
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
    file.write((char*)(meshData.vertices().data()), vertexCount * sizeof(MeshUtils::OBJMeshData::Vertex));
    file.write((char*)(meshData.indices().data()), indexCount * sizeof(MeshUtils::OBJMeshData::Index));
    return true;
}

bool MeshUtils::loadMeshData(const std::string& filePath, MeshUtils::OBJMeshData& meshData) {
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

size_t MeshUtils::getPolygonCount(const size_t& numIndices, const MeshPrimitiveType& primitiveType)  {
    switch (primitiveType) {
        case PrimitiveType_Point: return numIndices;
        case PrimitiveType_Line: return numIndices / 2;
        case PrimitiveType_Triangle: return numIndices / 3;
        default:
            assert(false); // Invalid primitive type
            return 0;
    }
}

