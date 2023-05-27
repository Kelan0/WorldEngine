#ifndef WORLDENGINE_TERRAINTILEQUADTREE_H
#define WORLDENGINE_TERRAINTILEQUADTREE_H

#include "core/core.h"
#include "core/engine/scene/Transform.h"

class Frustum;

class TerrainTileQuadtree {
public:
    enum QuadIndex : uint8_t {
        QuadIndex_TopLeft = 0,     // 0b00 // Binary digits of index indicate quadrant
        QuadIndex_BottomLeft = 1,  // 0b01
        QuadIndex_TopRight = 2,    // 0b10
        QuadIndex_BottomRight = 3, // 0b11
    };

    static std::array<glm::uvec2, 4> QUAD_OFFSETS;

    struct TileTreeNode {
        uint32_t childOffset;
        union {
            uint8_t _packed0;
            struct {
                uint8_t treeDepth : 6; // 6-bits, max depth is 63
                uint8_t deleted : 1;
            };
        };
        glm::uvec2 treePosition;
        QuadIndex quadIndex;
    };

public:
    TerrainTileQuadtree(uint32_t maxQuadtreeDepth, const glm::dvec2& size, double heightScale);

    ~TerrainTileQuadtree();

    void update(const Frustum* frustum);

    const std::vector<TileTreeNode>& getNodes();

    glm::dvec2 getNormalizedNodeCoordinate(const glm::dvec2& treePosition, uint8_t treeDepth);

    double getNormalizedNodeSizeForTreeDepth(uint8_t treeDepth);

    glm::dvec3 getNodePosition(const glm::dvec2& normalizedNodeCoordinate);

    glm::dvec2 getNodeSize(uint8_t treeDepth);

    const Transform& getTransform() const;

    void setTransform(const Transform& transform);

    uint32_t getMaxQuadtreeDepth() const;

    void setMaxQuadtreeDepth(uint32_t maxQuadtreeDepth);

    const glm::dvec2& getSize() const;

    void setSize(const glm::dvec2& size);

    double getHeightScale() const;

    void setHeightScale(double heightScale);

    static bool hasChildren(const TileTreeNode& node);

    static bool isDeleted(const TileTreeNode& node);

private:
    size_t splitNode(size_t nodeIndex);

    size_t mergeNode(size_t nodeIndex);

private:
    enum NodeUpdateType {
        NodeUpdateType_Split = 0,
        NodeUpdateType_Merge = 1,
    };
    struct NodeUpdate {
        size_t index;
        double distance;
        uint8_t treeDepth;
    };
private:
    Transform m_transform;
    uint32_t m_maxQuadtreeDepth;
    glm::dvec2 m_size;
    double m_heightScale;
    std::vector<TileTreeNode> m_nodes;
};


#endif //WORLDENGINE_TERRAINTILEQUADTREE_H
