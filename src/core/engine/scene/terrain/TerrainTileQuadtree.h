#ifndef WORLDENGINE_TERRAINTILEQUADTREE_H
#define WORLDENGINE_TERRAINTILEQUADTREE_H

#include "core/core.h"
#include "core/engine/scene/Transform.h"

class Frustum;
class TerrainTileSupplier;
class AxisAlignedBoundingBox;
class TileDataReference;

class TerrainTileQuadtree {
    NO_COPY(TerrainTileQuadtree);
    NO_MOVE(TerrainTileQuadtree);
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
                uint8_t visible : 1;
            };
        };
    };

    struct TraversalInfo {
        size_t nodeIndex;
        glm::uvec2 treePosition;
        uint8_t treeDepth;
        QuadIndex quadIndex;
    };

public:
    TerrainTileQuadtree(uint32_t maxQuadtreeDepth, const glm::dvec2& size, double heightScale);

    ~TerrainTileQuadtree();

    void update(const Frustum* frustum);

    template<class Fn>
    void traverseTreeNodes(std::vector<TraversalInfo>& traversalStack, Fn callback);

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

    bool hasChildren(size_t nodeIndex);

    bool isDeleted(size_t nodeIndex);

    bool isVisible(size_t nodeIndex);

    size_t getChildIndex(size_t nodeIndex, QuadIndex quadIndex);

    AxisAlignedBoundingBox getNodeBoundingBox(size_t nodeIndex, const glm::uvec2& treePosition, uint8_t treeDepth) const;

    Visibility calculateNodeVisibility(const Frustum* frustum, size_t nodeIndex, const glm::uvec2& treePosition, uint8_t treeDepth) const;

private:
    void updateSubdivisions(const Frustum* frustum, std::vector<size_t>& deletedNodeIndices, std::vector<TraversalInfo>& traversalStack);

    void updateVisibility(const Frustum* frustum, std::vector<TraversalInfo>& traversalStack, std::vector<size_t>& fullyVisibleSubtrees);

    void updatePriorities(std::vector<TraversalInfo>& traversalStack);

    void markVisibleSubtrees(std::vector<size_t>& visibleNodeIndices, std::vector<size_t>& unvisitedNodesStack);

    void markDeletedSubtrees(std::vector<size_t>& deletedNodeIndices, std::vector<size_t>& unvisitedNodesStack);

    void cleanupDeletedNodes();

    size_t splitNode(size_t nodeIndex, const glm::uvec2& treePosition, uint8_t treeDepth);

    size_t mergeNode(size_t nodeIndex);

private:
    struct NodeData {
        float minElevation = 0.0F;
        float maxElevation = 1.0F;
    };
private:
    Transform m_transform;
    uint32_t m_maxQuadtreeDepth;
    glm::dvec2 m_size;
    double m_heightScale;
    double m_splitThreshold;
    std::vector<TileTreeNode> m_nodes;
    std::vector<uint32_t> m_parentOffsets;
    std::vector<NodeData> m_nodeTileData;
    std::shared_ptr<TerrainTileSupplier> m_tileSupplier;
};



template<class Fn>
void TerrainTileQuadtree::traverseTreeNodes(std::vector<TraversalInfo>& traversalStack, Fn callback) {

    TraversalInfo traversalInfo{};
    traversalInfo.nodeIndex = 0;
    traversalInfo.treePosition = glm::uvec2(0, 0);
    traversalInfo.treeDepth = 0;
    traversalInfo.quadIndex = (QuadIndex)0;

    traversalStack.clear();
    traversalStack.emplace_back(traversalInfo);

    while (!traversalStack.empty()) {
        traversalInfo = traversalStack.back();
        traversalStack.pop_back();

        assert(!isDeleted(traversalInfo.nodeIndex));

        if constexpr(std::is_void_v<std::invoke_result_t<Fn, decltype(this), decltype(traversalInfo)>>) {
            callback(this, traversalInfo);
        } else {
            bool ignoreSubtree = callback(this, traversalInfo);
            if (ignoreSubtree)
                continue;
        }

        // Callback could have caused our node reference above to become invalid. Access via index from this point on.
        if (!hasChildren(traversalInfo.nodeIndex))
            continue;

        size_t firstChildIndex = traversalInfo.nodeIndex + m_nodes[traversalInfo.nodeIndex].childOffset;

        for (int i = 3; i >= 0; --i) { // Push children in reverse order, so they are popped in ascending order.
            traversalStack.emplace_back(TraversalInfo{
                .nodeIndex = firstChildIndex + i,
                .treePosition = traversalInfo.treePosition * 2u + QUAD_OFFSETS[i],
                .treeDepth = (uint8_t)(traversalInfo.treeDepth + 1u),
                .quadIndex = (QuadIndex)i
            });
        }
    }
}


#endif //WORLDENGINE_TERRAINTILEQUADTREE_H
