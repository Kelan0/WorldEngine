#ifndef WORLDENGINE_TERRAINTILEQUADTREE_H
#define WORLDENGINE_TERRAINTILEQUADTREE_H

#include "core/core.h"
#include "core/engine/scene/Transform.h"

class Frustum;

class TerrainTileQuadtree {
public:
    enum QuadIndex {
        QuadIndex_TopLeft = 0,     // 0b00 // Binary digits of index indicate quadrant
        QuadIndex_BottomLeft = 1,  // 0b01
        QuadIndex_TopRight = 2,    // 0b10
        QuadIndex_BottomRight = 3, // 0b11
    };

    static std::array<glm::uvec2, 4> QUAD_OFFSETS;

    struct NodeQuad {
        uint8_t treeDepth;
        glm::uvec2 treePosition;
        glm::dvec2 normalizedCoord;
        double normalizedSize;
    };

private:
    struct TileTreeNode;

public:
    TerrainTileQuadtree(uint32_t maxQuadtreeDepth, const glm::dvec2& size, double heightScale);

    ~TerrainTileQuadtree();

    void update(const Frustum* frustum);

    const std::vector<NodeQuad>& getLeafNodeQuads();

    glm::dvec2 getNormalizedNodeCoordinate(size_t nodeIndex, const glm::dvec2& offset);

    double getNormalizedNodeSizeForTreeDepth(uint8_t treeDepth);

    glm::dvec3 getNodePosition(const glm::dvec2& normalizedNodeCoordinate);

    glm::dvec2 getNodeSize(uint8_t treeDepth);

    NodeQuad getNodeQuad(size_t nodeIndex);

    const Transform& getTransform() const;

    void setTransform(const Transform& transform);

    uint32_t getMaxQuadtreeDepth() const;

    void setMaxQuadtreeDepth(uint32_t maxQuadtreeDepth);

    const glm::dvec2& getSize() const;

    void setSize(const glm::dvec2& size);

    double getHeightScale() const;

    void setHeightScale(double heightScale);

private:
    void splitNode(size_t nodeIndex);

    void mergeNode(size_t nodeIndex);

private:
    struct TileTreeNode {
        uint32_t parentOffset; // Parent node is always before the child nodes in the array. parentOffset is the number of indices before this node to get to the parent
        uint32_t childOffset; // Child nodes are always after the parent node in the array. childOffset is the number of indices after this node to get to the first of 4 children. All 4 are consecutive.
        uint8_t treeDepth;
        glm::uvec2 treePosition;
        QuadIndex quadIndex;
    };

private:
    Transform m_transform;
    uint32_t m_maxQuadtreeDepth;
    glm::dvec2 m_size;
    double m_heightScale;
    std::vector<TileTreeNode> m_nodes;
    std::vector<NodeQuad> m_leafNodeQuads;
};


#endif //WORLDENGINE_TERRAINTILEQUADTREE_H
