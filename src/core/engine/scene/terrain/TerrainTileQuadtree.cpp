#include "core/engine/scene/terrain/TerrainTileQuadtree.h"
#include "core/engine/scene/terrain/TerrainTileSupplier.h"
#include "core/engine/scene/bound/BoundingVolume.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/scene/Entity.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/util/Logger.h"
#include "core/util/Profiler.h"

#define DEBUG_RENDER_BOUNDING_VOLUMES 1


// These must match the order of the QuadIndex enum
std::array<glm::uvec2, 4> TerrainTileQuadtree::QUAD_OFFSETS = {
        glm::uvec2(0, 0), // QuadIndex_TopLeft
        glm::uvec2(0, 1), // QuadIndex_BottomLeft
        glm::uvec2(1, 0), // QuadIndex_TopRight
        glm::uvec2(1, 1)  // QuadIndex_BottomRight
};

TerrainTileQuadtree::TerrainTileQuadtree(uint32_t maxQuadtreeDepth, const glm::dvec2& size, double heightScale) :
        m_maxQuadtreeDepth(maxQuadtreeDepth),
        m_size(size),
        m_heightScale(heightScale),
        m_splitThreshold(2.0),
        m_tileSupplier(nullptr) {

    TileTreeNode rootNode{};
    rootNode.childOffset = UINT32_MAX;
    m_nodes.emplace_back(rootNode);
    m_nodeTileData.emplace_back(NodeData{
        .minElevation = 0.0F,
        .maxElevation = 1.0F,
    });
}

TerrainTileQuadtree::~TerrainTileQuadtree() {

}

void TerrainTileQuadtree::update(const Frustum* frustum) {
    PROFILE_SCOPE("TerrainTileQuadtree::update");
    std::vector<size_t> tempNodeIndices;
    std::vector<size_t> unvisitedNodesStack;
    std::vector<TraversalInfo> traversalStack;

    // tempNodeIndices will contain all deleted subtrees
    updateSubdivisions(frustum, tempNodeIndices, traversalStack);

    markDeletedSubtrees(tempNodeIndices, unvisitedNodesStack);
    cleanupDeletedNodes();

    // tempNodeIndices will contain all fully visible subtrees
    tempNodeIndices.clear();
    updateVisibility(frustum, traversalStack, tempNodeIndices);
    markVisibleSubtrees(tempNodeIndices, unvisitedNodesStack);

    m_tileSupplier->update();
}

glm::dvec2 TerrainTileQuadtree::getNormalizedNodeCoordinate(const glm::dvec2& treePosition, uint8_t treeDepth) const {
    return treePosition * getNormalizedNodeSizeForTreeDepth(treeDepth);
}

double TerrainTileQuadtree::getNormalizedNodeSizeForTreeDepth(uint8_t treeDepth) const {
    return 1.0 / (double)(1 << treeDepth);
}

glm::dvec3 TerrainTileQuadtree::getNodePosition(const glm::dvec2& normalizedNodeCoordinate) const {
    return Transform::apply(m_transform, glm::dvec3((normalizedNodeCoordinate - glm::dvec2(0.5)) * m_size, 0.0F));
}

glm::dvec2 TerrainTileQuadtree::getNodeSize(uint8_t treeDepth) const {
    return m_size * getNormalizedNodeSizeForTreeDepth(treeDepth);
}

const Transform& TerrainTileQuadtree::getTransform() const {
    return m_transform;
}

void TerrainTileQuadtree::setTransform(const Transform& transform) {
    m_transform = transform;
}

uint32_t TerrainTileQuadtree::getMaxQuadtreeDepth() const {
    return m_maxQuadtreeDepth;
}

void TerrainTileQuadtree::setMaxQuadtreeDepth(uint32_t maxQuadtreeDepth) {
    m_maxQuadtreeDepth = maxQuadtreeDepth;
}

const glm::dvec2& TerrainTileQuadtree::getSize() const {
    return m_size;
}

void TerrainTileQuadtree::setSize(const glm::dvec2& size) {
    m_size = size;
}

double TerrainTileQuadtree::getHeightScale() const {
    return m_heightScale;
}

void TerrainTileQuadtree::setHeightScale(double heightScale) {
    m_heightScale = heightScale;
}

const std::shared_ptr<TerrainTileSupplier>& TerrainTileQuadtree::getTileSupplier() const {
    return m_tileSupplier;
}

void TerrainTileQuadtree::setTileSupplier(const std::shared_ptr<TerrainTileSupplier>& tileSupplier) {
    m_tileSupplier = tileSupplier;
}

bool TerrainTileQuadtree::hasChildren(size_t nodeIndex) {
    return m_nodes[nodeIndex].childOffset != UINT32_MAX;
}

bool TerrainTileQuadtree::isDeleted(size_t nodeIndex) {
    return m_nodes[nodeIndex].childOffset == 0;
//    return m_nodes[nodeIndex].deleted;
}

bool TerrainTileQuadtree::isVisible(size_t nodeIndex) {
    return m_nodes[nodeIndex].visible;
}

size_t TerrainTileQuadtree::getChildIndex(size_t nodeIndex, QuadIndex quadIndex) {
    return nodeIndex + m_nodes[nodeIndex].childOffset + quadIndex;
}

AxisAlignedBoundingBox TerrainTileQuadtree::getNodeBoundingBox(size_t nodeIndex, const glm::uvec2& treePosition, uint8_t treeDepth) const {
    double normalizedNodeSize = getNormalizedNodeSizeForTreeDepth(treeDepth);

    double minElevation = m_nodeTileData[nodeIndex].minElevation * m_heightScale;
    double maxElevation = m_nodeTileData[nodeIndex].maxElevation * m_heightScale;

//    if (m_tileSupplier != nullptr) {
//        TileDataReference tile = m_tileSupplier->getTile(glm::dvec2(treePosition) * normalizedNodeSize, glm::dvec2(normalizedNodeSize));
//        minElevation = tile.getMinHeight() * m_heightScale;
//        maxElevation = tile.getMaxHeight() * m_heightScale;
//    }

    double centerX = ((((double)treePosition.x + 0.5) * normalizedNodeSize) - 0.5) * m_size.x;
    double centerZ = ((((double)treePosition.y + 0.5) * normalizedNodeSize) - 0.5) * m_size.x;
    double centerY = (maxElevation + minElevation) * 0.5;

    double halfExtentX = normalizedNodeSize * m_size.x * 0.5;
    double halfExtentZ = normalizedNodeSize * m_size.y * 0.5;
    double halfExtentY = (maxElevation - minElevation) * 0.5;

    return AxisAlignedBoundingBox(glm::dvec3(centerX, centerY, centerZ), glm::dvec3(halfExtentX, halfExtentY, halfExtentZ));
}

TerrainTileQuadtree::Visibility TerrainTileQuadtree::calculateNodeVisibility(const Frustum* frustum, size_t nodeIndex, const glm::uvec2& treePosition, uint8_t treeDepth) const {
    AxisAlignedBoundingBox aabb = getNodeBoundingBox(nodeIndex, treePosition, treeDepth);

    Visibility result = Visibility_FullyVisible;

    glm::dvec3 corner;

    for (int i = 0; i < Frustum::NumPlanes; ++i) {
        const Plane& plane = frustum->getPlane(i);

        corner.x = plane.normal.x > 0 ? aabb.getBoundMaxX() : aabb.getBoundMinX();
        corner.y = plane.normal.y > 0 ? aabb.getBoundMaxY() : aabb.getBoundMinY();
        corner.z = plane.normal.z > 0 ? aabb.getBoundMaxZ() : aabb.getBoundMinZ();

        if (plane.calculateSignedDistance(corner) < 0.0)
            return Visibility_NotVisible;

        corner.x = plane.normal.x > 0 ? aabb.getBoundMinX() : aabb.getBoundMaxX();
        corner.y = plane.normal.y > 0 ? aabb.getBoundMinY() : aabb.getBoundMaxY();
        corner.z = plane.normal.z > 0 ? aabb.getBoundMinZ() : aabb.getBoundMaxZ();

        if (plane.calculateSignedDistance(corner) < 0.0)
            return Visibility_PartiallyVisible;
    }

    return result;
}

void TerrainTileQuadtree::updateSubdivisions(const Frustum* frustum, std::vector<size_t>& deletedNodeIndices, std::vector<TraversalInfo>& traversalStack) {
    PROFILE_SCOPE("TerrainTileQuadtree::updateSubdivisions")

    glm::dvec3 localCameraOrigin = glm::dvec3(glm::inverse(m_transform.getMatrix()) * glm::dvec4(frustum->getOrigin(), 1.0));
    localCameraOrigin.x += m_size.x * 0.5;
    localCameraOrigin.z += m_size.y * 0.5;

    double maxSize = glm::max(m_size.x, m_size.y);

    m_parentOffsets.clear();
    m_parentOffsets.resize(m_nodes.size(), UINT32_MAX);

    traverseTreeNodes(traversalStack, [&](TerrainTileQuadtree*, const TraversalInfo& node) {

        double normalizedNodeSize = getNormalizedNodeSizeForTreeDepth(node.treeDepth);
        glm::dvec2 normalizedNodeCenterCoord = (glm::dvec2(node.treePosition) + glm::dvec2(0.5)) * normalizedNodeSize;
        glm::dvec3 nodeCenterPos(normalizedNodeCenterCoord.x * m_size.x, 0.0, normalizedNodeCenterCoord.y * m_size.y);

        glm::dvec3 cameraToNodePosition = nodeCenterPos - localCameraOrigin;
        double cameraDistanceSq = glm::dot(cameraToNodePosition, cameraToNodePosition);

        double edgeSize = normalizedNodeSize * maxSize;
        double splitDistance = edgeSize * m_splitThreshold;

        if (cameraDistanceSq < splitDistance * splitDistance) {
            // Close enough to split

            if (!hasChildren(node.nodeIndex) && node.treeDepth < m_maxQuadtreeDepth) {
                // NOTE: splitNode here will grow the list, and potentially invalidate the node reference above.
                splitNode(node.nodeIndex, node.treePosition, node.treeDepth);
            }

        } else if (cameraDistanceSq > splitDistance * splitDistance) {
            // Far enough to merge

            if (hasChildren(node.nodeIndex)) {
                // NOTE: mergeNode is okay while iterating, it will not affect the size of the array or move anything around.
                size_t deletedChildIndex = mergeNode(node.nodeIndex);
                for (int i = 0; i < 4; ++i)
                    deletedNodeIndices.emplace_back(deletedChildIndex + i);
            }
        }

        glm::dvec2 tileOffset = glm::dvec2(node.treePosition * 2u + QUAD_OFFSETS[node.quadIndex]) * normalizedNodeSize;
        TileDataReference tile = m_tileSupplier->getTile(tileOffset, glm::dvec2(normalizedNodeSize));
        tile.notifyUsed();

        if (tile.isAvailable()) {
            m_nodeTileData[node.nodeIndex].minElevation = tile.getMinHeight();
            m_nodeTileData[node.nodeIndex].maxElevation = tile.getMaxHeight();
        } else {
            m_nodeTileData[node.nodeIndex].minElevation = 0.0F;
            m_nodeTileData[node.nodeIndex].maxElevation = 1.0F;
        }

        // node reference above may have been invalidated here, so we must index the nodes array directly
        if (hasChildren(node.nodeIndex)) {
            size_t firstChildIndex = getChildIndex(node.nodeIndex, (QuadIndex)0);
            assert(firstChildIndex <= m_nodes.size() - 4);
            for (int i = 3; i >= 0; --i) {
                size_t childIndex = firstChildIndex + i;
                m_parentOffsets[childIndex] = (uint32_t)(childIndex - node.nodeIndex);
            }
        } else {

//            if (m_nodeTileData[node.nodeIndex] == nullptr) {
//                glm::dvec2 tileOffset = glm::dvec2(node.treePosition * 2u + QUAD_OFFSETS[node.quadIndex]) * normalizedNodeSize;
//                m_tileSupplier->getTile(tileOffset, glm::dvec2(normalizedNodeSize));
//                m_nodeTileData[node.nodeIndex] = m_tileSupplier->getTile(tileOffset, glm::dvec2(normalizedNodeSize));
//            }
//            if (m_nodeTileData[node.nodeIndex] != nullptr) {
//                m_nodeTileData[node.nodeIndex].notifyUsed();
//            }
        }
        return false; // Traverse subtree
    });
}

void TerrainTileQuadtree::updateVisibility(const Frustum* frustum, std::vector<TraversalInfo>& traversalStack, std::vector<size_t>& fullyVisibleSubtrees) {
    PROFILE_SCOPE("TerrainTileQuadtree::updateVisibility")

#if DEBUG_RENDER_BOUNDING_VOLUMES
    Engine::instance()->getImmediateRenderer()->matrixMode(MatrixMode_Projection);
    Engine::instance()->getImmediateRenderer()->pushMatrix();
    Engine::instance()->getImmediateRenderer()->loadMatrix(Engine::scene()->getMainCameraEntity().getComponent<Camera>().getProjectionMatrix());
    Engine::instance()->getImmediateRenderer()->matrixMode(MatrixMode_ModelView);
    Engine::instance()->getImmediateRenderer()->pushMatrix();
    Engine::instance()->getImmediateRenderer()->loadMatrix(glm::inverse(glm::mat4(Engine::scene()->getMainCameraEntity().getComponent<Transform>().getMatrix())));

    Engine::instance()->getImmediateRenderer()->setCullMode(vk::CullModeFlagBits::eNone);
    Engine::instance()->getImmediateRenderer()->setColourBlendMode(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);

    Engine::instance()->getImmediateRenderer()->setLineWidth(1.0F);
    Engine::instance()->getImmediateRenderer()->setBlendEnabled(false);
    Engine::instance()->getImmediateRenderer()->setDepthTestEnabled(false);
#endif

    Frustum localFrustum = Frustum::transform(*frustum, glm::inverse(m_transform.getMatrix()));

    for (auto& node : m_nodes)
        node.visible = false;

    traverseTreeNodes(traversalStack, [&](TerrainTileQuadtree*, const TraversalInfo& traversalInfo) {
        TileTreeNode& node = m_nodes[traversalInfo.nodeIndex];

        Visibility visibility = calculateNodeVisibility(&localFrustum, traversalInfo.nodeIndex, traversalInfo.treePosition, traversalInfo.treeDepth);
        node.visible = visibility != Visibility_NotVisible;

#if DEBUG_RENDER_BOUNDING_VOLUMES
        if (visibility == Visibility_FullyVisible)
            Engine::instance()->getImmediateRenderer()->colour(0.2F, 1.0F, 0.2F, 1.0F);
        else if (visibility == Visibility_PartiallyVisible)
            Engine::instance()->getImmediateRenderer()->colour(1.0F, 1.0F, 0.2F, 1.0F);
        else if (visibility == Visibility_NotVisible)
            Engine::instance()->getImmediateRenderer()->colour(1.0F, 0.2F, 0.2F, 1.0F);
        getNodeBoundingBox(traversalInfo.nodeIndex, traversalInfo.treePosition, traversalInfo.treeDepth).drawLines();
#endif

        if (visibility == Visibility_FullyVisible)
            fullyVisibleSubtrees.emplace_back(traversalInfo.nodeIndex);

        return visibility != Visibility_PartiallyVisible; // Skip subtrees that are fully visible or fully invisible
    });

#if DEBUG_RENDER_BOUNDING_VOLUMES
    Engine::instance()->getImmediateRenderer()->popMatrix(MatrixMode_ModelView);
    Engine::instance()->getImmediateRenderer()->popMatrix(MatrixMode_Projection);
#endif
}

void TerrainTileQuadtree::markVisibleSubtrees(std::vector<size_t>& visibleNodeIndices, std::vector<size_t>& unvisitedNodesStack) {
    unvisitedNodesStack.clear();
    for (const size_t& nodeIndex : visibleNodeIndices)
        unvisitedNodesStack.emplace_back(nodeIndex);

    while (!unvisitedNodesStack.empty()) {
        size_t nodeIndex = unvisitedNodesStack.back();
        unvisitedNodesStack.pop_back();

        TileTreeNode& node = m_nodes[nodeIndex];
        node.visible = true;

        if (!hasChildren(nodeIndex))
            continue;

        for (int i = 3; i >= 0; --i)
            unvisitedNodesStack.emplace_back(getChildIndex(nodeIndex, (QuadIndex)i));
    }
}

void TerrainTileQuadtree::markDeletedSubtrees(std::vector<size_t>& deletedNodeIndices, std::vector<size_t>& unvisitedNodesStack) {
    PROFILE_SCOPE("TerrainTileQuadtree::markDeletedSubtrees")

    unvisitedNodesStack.clear();
    for (const size_t& nodeIndex : deletedNodeIndices)
        unvisitedNodesStack.emplace_back(nodeIndex);

    while (!unvisitedNodesStack.empty()) {
        size_t nodeIndex = unvisitedNodesStack.back();
        unvisitedNodesStack.pop_back();

        if (hasChildren(nodeIndex)) {
            for (int i = 3; i >= 0; --i)
                unvisitedNodesStack.emplace_back(getChildIndex(nodeIndex, (QuadIndex)i));
        }

//        m_nodes[nodeIndex].deleted = true;
        m_nodes[nodeIndex].childOffset = 0;
//        m_nodeTileData[nodeIndex].release();
    }
}

void TerrainTileQuadtree::cleanupDeletedNodes() {
    PROFILE_SCOPE("TerrainTileQuadtree::cleanupDeletedNodes")

//    auto t0 = Time::now();

    size_t firstIndex = 0;
    for (; firstIndex < m_nodes.size() && !isDeleted(firstIndex); ++firstIndex);

    if (firstIndex < m_nodes.size()) {
        for (size_t i = firstIndex; i < m_nodes.size(); ++i) {
            if (!isDeleted(i)) {
                uint32_t offset = (uint32_t)(i - firstIndex);

                assert(m_parentOffsets[i] != UINT32_MAX);

                size_t parentIndex = i - m_parentOffsets[i];
                assert(m_nodes[parentIndex].childOffset < m_nodes.size() - parentIndex);

                if (hasChildren(i)) {
                    for (int j = 0; j < 4; ++j) {
                        size_t childIndex = i + m_nodes[i].childOffset + j;
                        size_t childParentIndex = childIndex - m_parentOffsets[childIndex];
                        assert(childParentIndex + m_nodes[childParentIndex].childOffset == childIndex - j);
                        m_parentOffsets[childIndex] += offset;
                    }
                    m_nodes[i].childOffset += offset;
                }

                // This node is the first child of its parent. The offset from the parent to this node needs to shift accordingly.
                if (parentIndex + m_nodes[parentIndex].childOffset == i) {
                    assert(m_nodes[parentIndex].childOffset > offset);
//                    assert(parentIndex + m_nodes[parentIndex].childOffset == i - m_nodes[i].quadIndex);
                    m_nodes[parentIndex].childOffset -= offset;
                }

                // Node moves from index i to firstIndex, the offset from this node to its parent needs to decrement accordingly
                if (parentIndex < firstIndex) {
                    m_parentOffsets[i] -= offset;
                    assert(m_parentOffsets[i] <= firstIndex);
                }

                // element at index i moves to firstIndex
#if 0
                std::swap(m_nodes[firstIndex], m_nodes[i]);
                std::swap(m_parentOffsets[firstIndex], m_parentOffsets[i]);
                std::swap(m_nodeTileData[firstIndex], m_nodeTileData[i]);
#else
                m_nodes[firstIndex] = m_nodes[i];
                m_parentOffsets[firstIndex] = m_parentOffsets[i];
                m_nodeTileData[firstIndex] = std::move(m_nodeTileData[i]);
#endif

#if _DEBUG
                if (m_parentOffsets[firstIndex] != UINT32_MAX) {
                    size_t testParentIndex = firstIndex - m_parentOffsets[firstIndex];
                    assert(m_nodes[testParentIndex].childOffset <= firstIndex - testParentIndex);
//                    assert(testParentIndex + m_nodes[testParentIndex].childOffset == firstIndex - m_nodes[firstIndex].quadIndex);
                }
#endif

                ++firstIndex;
            }
        }

//        LOG_DEBUG("Removing %zu deleted nodes in %.2f msec", (size_t)m_nodes.size() - firstIndex, Time::milliseconds(t0));

        PROFILE_REGION("Erase deleted nodes")
        m_nodes.erase(m_nodes.begin() + (decltype(m_nodes)::difference_type)firstIndex, m_nodes.end());
        m_parentOffsets.erase(m_parentOffsets.begin() + (decltype(m_parentOffsets)::difference_type)firstIndex, m_parentOffsets.end());
        m_nodeTileData.erase(m_nodeTileData.begin() + (decltype(m_nodeTileData)::difference_type)firstIndex, m_nodeTileData.end());


#if _DEBUG
        assert(m_nodes.size() == m_parentOffsets.size());
        assert(m_nodeTileData.size() == m_parentOffsets.size());

        // SANITY CHECK - every entry in the m_parentOffsets array must make sense.
        for (size_t i = 1; i < m_nodes.size(); ++i) {
            assert(!isDeleted(i));
            size_t parentIndex = i - m_parentOffsets[i];
            size_t parentFirstChildIndex = parentIndex + m_nodes[parentIndex].childOffset;
            assert(i >= parentFirstChildIndex && i < parentFirstChildIndex + 4);
        }
#endif
    }

    if (m_nodes.capacity() > m_nodes.size() * 4) {
        LOG_DEBUG("Deallocating unused memory for %zu quadtree terrain nodes", m_nodes.capacity() - m_nodes.size());
        m_nodes.shrink_to_fit();
        m_parentOffsets.shrink_to_fit();
        m_nodeTileData.shrink_to_fit();
    }
}

size_t TerrainTileQuadtree::splitNode(size_t nodeIndex, const glm::uvec2& treePosition, uint8_t treeDepth) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::splitNode - node index out of range");

    TileTreeNode& node = m_nodes[nodeIndex];
    assert(node.childOffset == UINT32_MAX && "TerrainTileQuadtree::splitNode - node already split");

    assert(!isDeleted(nodeIndex));

    if (hasChildren(nodeIndex))
        return nodeIndex + node.childOffset; // Node already split, return index of first child

    size_t firstChildIndex = m_nodes.size();

//    LOG_DEBUG("Splitting node %zu [%u %u %u] - children at %zu", nodeIndex, node.treePosition.x, node.treePosition.y, node.treeDepth, firstChildIndex);

    node.childOffset = (uint32_t)(firstChildIndex - nodeIndex);

    TileTreeNode childNode{};
    childNode.childOffset = UINT32_MAX;

    double normalizedNodeSize = getNormalizedNodeSizeForTreeDepth(treeDepth + 1);
    glm::dvec2 tileSize = glm::dvec2(normalizedNodeSize);

    // Note: node reference is invalidated by the addition of new children below.
    for (int i = 0; i < 4; ++i) {
        glm::dvec2 tileOffset = glm::dvec2(treePosition * 2u + QUAD_OFFSETS[i]) * normalizedNodeSize;
        TileDataReference tile = m_tileSupplier->getTile(tileOffset, tileSize);
        tile.notifyUsed();

        m_nodeTileData.emplace_back(NodeData{
            .minElevation = 0.0F,
            .maxElevation = 1.0F
        });
        m_parentOffsets.emplace_back((uint32_t)(m_nodes.size() - nodeIndex)); // Offset to subtract from this child nodes index to reach its parent.
        m_nodes.emplace_back(childNode);
    }

    return firstChildIndex;
}

size_t TerrainTileQuadtree::mergeNode(size_t nodeIndex) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::mergeNode - node index out of range");

    TileTreeNode& node = m_nodes[nodeIndex];
    assert(!isDeleted(nodeIndex));

    if (!hasChildren(nodeIndex)) {
        return SIZE_MAX; // Node has no children. Nothing to merge.
    }

    size_t firstChildIndex = nodeIndex + node.childOffset;

//    LOG_DEBUG("Merging node %zu [%u %u %u] - children at %lld", nodeIndex, node.treePosition.x, node.treePosition.y, node.treeDepth, firstChildIndex);

    // Set the child offset of this node to UINT32_MAX, this will orphan the child subtrees, and they will get cleaned up later.
    node.childOffset = UINT32_MAX;


    return firstChildIndex;
}
