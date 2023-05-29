#ifndef WORLDENGINE_TERRAINTILEQUADTREE_H
#define WORLDENGINE_TERRAINTILEQUADTREE_H

#include "core/core.h"
#include "core/engine/scene/Transform.h"

class Frustum;
class TerrainTileSupplier;

class TerrainTileQuadtree {
public:
    enum QuadIndex : uint8_t {
        QuadIndex_TopLeft = 0,     // 0b00 // Binary digits of index indicate quadrant
        QuadIndex_BottomLeft = 1,  // 0b01
        QuadIndex_TopRight = 2,    // 0b10
        QuadIndex_BottomRight = 3, // 0b11
    };

    enum Visibility {
        Visibility_NotVisible = 0,
        Visibility_PartiallyVisible = 1,
        Visibility_FullyVisible = 2,
    };

    static std::array<glm::uvec2, 4> QUAD_OFFSETS;

    struct TileTreeNode {
        uint32_t childOffset;
        union {
            uint8_t _packed0;
            struct {
                uint8_t treeDepth : 6; // 6-bits, max depth is 63
                uint8_t deleted : 1;
                uint8_t visible : 1;
            };
        };
        glm::uvec2 treePosition;
        QuadIndex quadIndex;
    };

public:
    TerrainTileQuadtree(uint32_t maxQuadtreeDepth, const glm::dvec2& size, double heightScale);

    ~TerrainTileQuadtree();

    void update(const Frustum* frustum);

    const TileTreeNode& getNode(size_t index) const;

    template<class Fn>
    void forEachLeafNode(Fn callback);

    template<class Fn>
    void traverseTreeNodes(std::vector<size_t>& unvisitedNodesStack, Fn callback);

    glm::dvec2 getNormalizedNodeCoordinate(const glm::dvec2& treePosition, uint8_t treeDepth) const;

    double getNormalizedNodeSizeForTreeDepth(uint8_t treeDepth) const;

    glm::dvec3 getNodePosition(const glm::dvec2& normalizedNodeCoordinate) const;

    glm::dvec2 getNodeSize(uint8_t treeDepth) const;

    const Transform& getTransform() const;

    void setTransform(const Transform& transform);

    uint32_t getMaxQuadtreeDepth() const;

    void setMaxQuadtreeDepth(uint32_t maxQuadtreeDepth);

    const glm::dvec2& getSize() const;

    void setSize(const glm::dvec2& size);

    double getHeightScale() const;

    void setHeightScale(double heightScale);

    const std::shared_ptr<TerrainTileSupplier>& getTileSupplier() const;

    void setTileSupplier(const std::shared_ptr<TerrainTileSupplier>& tileSupplier);

    static bool hasChildren(const TileTreeNode& node);

    static bool isDeleted(const TileTreeNode& node);

    static bool isVisible(const TileTreeNode& node);

private:
    void updateSubdivisions(const Frustum* frustum, std::vector<size_t>& deletedNodeIndices, std::vector<size_t>& unvisitedNodesStack);

    void updateVisibility(const Frustum* frustum, std::vector<size_t>& unvisitedNodesStack);

    void markAllDeletedSubtrees(std::vector<size_t>& deletedNodeIndices, std::vector<size_t>& unvisitedNodesStack);

    void cleanupDeletedNodes();

    Visibility calculateNodeVisibility(const Frustum* frustum, const TileTreeNode& node) const;

    size_t splitNode(size_t nodeIndex);

    size_t mergeNode(size_t nodeIndex);

private:
    Transform m_transform;
    uint32_t m_maxQuadtreeDepth;
    glm::dvec2 m_size;
    double m_heightScale;
    double m_splitThreshold;
    std::vector<TileTreeNode> m_nodes;
    std::vector<uint32_t> m_parentOffsets;
    std::shared_ptr<TerrainTileSupplier> m_tileSupplier;
};




template<class Fn>
void TerrainTileQuadtree::forEachLeafNode(Fn callback) {
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const TileTreeNode& node = m_nodes[i]; // Node must not be modified

        if (hasChildren(node))
            continue;

        if constexpr(std::is_void_v<std::invoke_result_t<Fn, decltype(this), decltype(node), decltype(i)>>) {
            callback(this, node, i);
        } else {
            bool breakLoop = callback(this, node, i);
            if (breakLoop)
                break;
        }
    }
}

template<class Fn>
void TerrainTileQuadtree::traverseTreeNodes(std::vector<size_t>& unvisitedNodesStack, Fn callback) {
    unvisitedNodesStack.clear();
    unvisitedNodesStack.emplace_back(0); // root

    while (!unvisitedNodesStack.empty()) {
        size_t nodeIndex = unvisitedNodesStack.back();
        unvisitedNodesStack.pop_back();

        const TileTreeNode& node = m_nodes[nodeIndex];

        if constexpr(std::is_void_v<std::invoke_result_t<Fn, decltype(this), decltype(node), decltype(nodeIndex)>>) {
            callback(this, node, nodeIndex);
        } else {
            bool ignoreSubtree = callback(this, node, nodeIndex);
            if (ignoreSubtree)
                continue;
        }

        // Callback could have caused our node reference above to become invalid. Access via index from this point on.
        if (!hasChildren(m_nodes[nodeIndex]))
            continue;

        for (int i = 3; i >= 0; --i) // Push children in reverse order, so they are popped in ascending order.
            unvisitedNodesStack.emplace_back(nodeIndex + m_nodes[nodeIndex].childOffset + i);
    }
}


#endif //WORLDENGINE_TERRAINTILEQUADTREE_H
