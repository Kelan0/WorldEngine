
#include "../../core.h"

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
		Index baseTriangle = 0;
	};

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

	void applyTransform(bool currentStateOnly = true);

	void pushState();

	void popState();

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

	Index addTriangle(Index	i0, Index i1, Index i2);

	Index addTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

	Index addQuad(Index	i0, Index i1, Index i2, Index i3);

	Index addQuad(const Vertex& v0, const Vertex& v1, const Vertex& v2, const Vertex& v3);

	const std::vector<Vertex>& getVertices() const;

	const std::vector<Triangle>& getTriangles() const;

	glm::vec3 calculateCenterOffset(bool currentStateOnly = true) const;

	// Transforming a vector where any component is 0 or 1 by this matrix will result in a point on the bounding box.
	// e.g. (-1,-1,-1) will be BB min, (+1,+1,+1) will be BB max, (0,+1,0) will be center top etc.
	glm::mat4 calculateBoundingBox(bool currentStateOnly = true) const;

	size_t getVertexCount() const;

	size_t getIndexCount() const;

	size_t getPolygonCount() const;

private:
	std::vector<Vertex> m_vertices;
	std::vector<Triangle> m_triangles;
	std::stack<glm::mat4> m_transfromStack;
	glm::mat4 m_currentTransform;
	std::stack<State> m_stateStack;
	State m_currentState;
};



namespace MeshLoader {
	bool loadOBJ(const std::string& filePath, MeshData& meshData);
};