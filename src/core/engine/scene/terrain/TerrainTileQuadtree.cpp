#include "TerrainTileQuadtree.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/util/Logger.h"
#include "core/util/Profiler.h"
#include "core/engine/scene/Entity.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/renderer/ImmediateRenderer.h"

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
        m_splitThreshold(2.0) {

    TileTreeNode rootNode{};
//    rootNode.parentOffset = UINT32_MAX;
    rootNode.childOffset = UINT32_MAX;
    rootNode.treeDepth = 0;
    rootNode.treePosition = glm::uvec2(0, 0);
    rootNode.quadIndex = (QuadIndex)0;
    m_nodes.emplace_back(rootNode);

//    size_t rootChildIndex = splitNode(0);
//
//    size_t indexTopLeft = rootChildIndex + QuadIndex_TopLeft;
//    size_t indexBottomLeft = rootChildIndex + QuadIndex_BottomLeft;
//    size_t indexTopRight = rootChildIndex + QuadIndex_TopRight;
//    size_t indexBottomRight = rootChildIndex + QuadIndex_BottomRight;
//
//    for (int i = 0; i < 8; ++i) {
//        indexTopLeft = splitNode(indexTopLeft) + QuadIndex_BottomRight;
//        indexBottomLeft = splitNode(indexBottomLeft) + QuadIndex_TopRight;
//        indexTopRight = splitNode(indexTopRight) + QuadIndex_BottomLeft;
//        indexBottomRight = splitNode(indexBottomRight) + QuadIndex_TopLeft;
//    }
}

TerrainTileQuadtree::~TerrainTileQuadtree() {

}

void TerrainTileQuadtree::update(const Frustum* frustum) {
    PROFILE_SCOPE("TerrainTileQuadtree::update");
    std::vector<size_t> deletedNodeIndices;
    std::vector<size_t> unvisitedNodes;

    updateSubdivisions(frustum, deletedNodeIndices, unvisitedNodes);

    markAllDeletedSubtrees(deletedNodeIndices, unvisitedNodes);

    cleanupDeletedNodes();

    if (m_nodes.capacity() > m_nodes.size() * 4) {
        LOG_DEBUG("Deallocating unused memory for %zu quadtree terrain nodes", m_nodes.capacity() - m_nodes.size());
        m_nodes.shrink_to_fit();
        m_parentOffsets.shrink_to_fit();
    }

    updateVisibility(frustum, unvisitedNodes);
}

const TerrainTileQuadtree::TileTreeNode& TerrainTileQuadtree::getNode(size_t index) const {
    assert(index < m_nodes.size());
    return m_nodes[index];
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

bool TerrainTileQuadtree::hasChildren(const TileTreeNode& node) {
    return node.childOffset != UINT32_MAX;
}

bool TerrainTileQuadtree::isDeleted(const TileTreeNode& node) {
//    return node.childOffset == 0;
    return node.deleted;
}

bool TerrainTileQuadtree::isVisible(const TileTreeNode& node) {
    return node.visible;
}


void TerrainTileQuadtree::updateSubdivisions(const Frustum* frustum, std::vector<size_t>& deletedNodeIndices, std::vector<size_t>& unvisitedNodesStack) {
    PROFILE_SCOPE("TerrainTileQuadtree::updateSubdivisions")

    glm::dvec3 localCameraOrigin = glm::dvec3(glm::inverse(m_transform.getMatrix()) * glm::dvec4(frustum->getOrigin(), 1.0));
    localCameraOrigin.x += m_size.x * 0.5;
    localCameraOrigin.z += m_size.y * 0.5;

    double maxSize = glm::max(m_size.x, m_size.y);

    m_parentOffsets.clear();
    m_parentOffsets.resize(m_nodes.size(), UINT32_MAX);

    traverseTreeNodes(unvisitedNodesStack, [&](TerrainTileQuadtree*, const TileTreeNode& node, size_t nodeIndex) {
        if (isDeleted(node)) {
            deletedNodeIndices.emplace_back(nodeIndex);
            return true; // Skip the subtree
        }

        double normalizedNodeSize = getNormalizedNodeSizeForTreeDepth(node.treeDepth);
        glm::dvec2 normalizedNodeCenterCoord = (glm::dvec2(node.treePosition) + glm::dvec2(0.5)) * normalizedNodeSize;
        glm::dvec3 nodeCenterPos(normalizedNodeCenterCoord.x * m_size.x, 0.0, normalizedNodeCenterCoord.y * m_size.y);

        glm::dvec3 cameraToNodePosition = nodeCenterPos - localCameraOrigin;
        double cameraDistanceSq = glm::dot(cameraToNodePosition, cameraToNodePosition);

        double edgeSize = normalizedNodeSize * maxSize;
        double splitDistance = edgeSize * m_splitThreshold;

        if (cameraDistanceSq < splitDistance * splitDistance) {
            // Close enough to split

            if (!hasChildren(node) && node.treeDepth < m_maxQuadtreeDepth) {
                // NOTE: splitNode here will grow the list, and potentially invalidate the node reference above.
                splitNode(nodeIndex);
            }

        } else if (cameraDistanceSq > splitDistance * splitDistance) {
            // Far enough to merge

            if (hasChildren(node)) {
                // NOTE: mergeNode is okay while iterating, it will not affect the size of the array or move anything around.
                size_t deletedChildIndex = mergeNode(nodeIndex);
                for (int i = 0; i < 4; ++i)
                    deletedNodeIndices.emplace_back(deletedChildIndex + i);
            }
        }

        // node reference above may have been invalidated here, so we must index the nodes array directly
        if (hasChildren(m_nodes[nodeIndex])) {
            size_t firstChildIndex = nodeIndex + m_nodes[nodeIndex].childOffset;
            assert(firstChildIndex <= m_nodes.size() - 4);
            for (int i = 3; i >= 0; --i) {
                size_t childIndex = firstChildIndex + i;
//                unvisitedNodesStack.emplace_back(childIndex);
                m_parentOffsets[childIndex] = (uint32_t)(childIndex - nodeIndex);
            }
        }
        return false; // Traverse subtree
    });
}

std::vector<BoundingSphere> testBoundingSpheres;

void TerrainTileQuadtree::updateVisibility(const Frustum* frustum, std::vector<size_t>& unvisitedNodesStack) {
    PROFILE_SCOPE("TerrainTileQuadtree::updateVisibility")

    PROFILE_REGION("Check visibility")

    Frustum localFrustum = Frustum::transform(*frustum, glm::inverse(m_transform.getMatrix()));

    for (auto& node : m_nodes)
        node.visible = false;

    std::vector<size_t> fullyVisibleSubtrees;

    unvisitedNodesStack.clear();
    unvisitedNodesStack.emplace_back(0);

    size_t traversalCount = 0;

    testBoundingSpheres.clear();

    while (!unvisitedNodesStack.empty()) {
        size_t nodeIndex = unvisitedNodesStack.back();
        unvisitedNodesStack.pop_back();

        TileTreeNode& node = m_nodes[nodeIndex];
        Visibility visibility = calculateNodeVisibility(&localFrustum, node);

        node.visible = visibility != Visibility_NotVisible;
        ++traversalCount;

        if (!hasChildren(node))
            continue;

        if (visibility == Visibility_PartiallyVisible) {
            for (int i = 3; i >= 0; --i) // Push children in reverse order, so they are popped in ascending order.
                unvisitedNodesStack.emplace_back(nodeIndex + m_nodes[nodeIndex].childOffset + i);

        } else if (visibility == Visibility_FullyVisible) {
            fullyVisibleSubtrees.emplace_back(nodeIndex);
        }
    }

    LOG_DEBUG("Visibility check traversed %zu nodes, %zu fully visible subtrees", traversalCount, fullyVisibleSubtrees.size());

    PROFILE_REGION("Update fully visible subtrees")

    unvisitedNodesStack.clear();
    for (const size_t& nodeIndex : fullyVisibleSubtrees)
        unvisitedNodesStack.emplace_back(nodeIndex);

    while (!unvisitedNodesStack.empty()) {
        size_t nodeIndex = unvisitedNodesStack.back();
        unvisitedNodesStack.pop_back();

        TileTreeNode& node = m_nodes[nodeIndex];
        node.visible = true;

        if (!hasChildren(node))
            continue;

        for (int i = 3; i >= 0; --i) // Push children in reverse order, so they are popped in ascending order.
            unvisitedNodesStack.emplace_back(nodeIndex + m_nodes[nodeIndex].childOffset + i);
    }

//    Entity mainCamera = Engine::scene()->getMainCameraEntity();
//    Transform& cameraTransform = mainCamera.getComponent<Transform>();
//    Camera& cameraProjection = mainCamera.getComponent<Camera>();
//
//    Engine::instance()->getImmediateRenderer()->matrixMode(MatrixMode_Projection);
//    Engine::instance()->getImmediateRenderer()->pushMatrix();
//    Engine::instance()->getImmediateRenderer()->loadMatrix(cameraProjection.getProjectionMatrix());
//    Engine::instance()->getImmediateRenderer()->matrixMode(MatrixMode_ModelView);
//    Engine::instance()->getImmediateRenderer()->pushMatrix();
//    Engine::instance()->getImmediateRenderer()->loadMatrix(glm::inverse(glm::mat4(cameraTransform.getMatrix())));
//
//    Engine::instance()->getImmediateRenderer()->setCullMode(vk::CullModeFlagBits::eNone);
//    Engine::instance()->getImmediateRenderer()->setColourBlendMode(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);
//
//    for (auto& bs : testBoundingSpheres) {
//        Engine::instance()->getImmediateRenderer()->setLineWidth(1.0F);
//        Engine::instance()->getImmediateRenderer()->setBlendEnabled(false);
//        Engine::instance()->getImmediateRenderer()->setDepthTestEnabled(false);
//        Engine::instance()->getImmediateRenderer()->colour(1.0F, 1.0F, 1.0F, 1.0F);
//        Engine::instance()->getImmediateRenderer()->setColourMultiplierEnabled(true);
//        Engine::instance()->getImmediateRenderer()->setBackfaceColourMultiplier(glm::vec4(0.4F, 1.0F, 0.4F, 0.2F));
//        bs.drawLines();
//    }
//
//    Engine::instance()->getImmediateRenderer()->popMatrix(MatrixMode_ModelView);
//    Engine::instance()->getImmediateRenderer()->popMatrix(MatrixMode_Projection);

    testBoundingSpheres.clear();
}

void TerrainTileQuadtree::markAllDeletedSubtrees(std::vector<size_t>& deletedNodeIndices, std::vector<size_t>& unvisitedNodesStack) {
    PROFILE_SCOPE("TerrainTileQuadtree::markAllDeletedSubtrees")

    unvisitedNodesStack.clear();
    for (const size_t& nodeIndex : deletedNodeIndices)
        unvisitedNodesStack.emplace_back(nodeIndex);

    while (!unvisitedNodesStack.empty()) {
        size_t nodeIndex = unvisitedNodesStack.back();
        unvisitedNodesStack.pop_back();

        TileTreeNode& node = m_nodes[nodeIndex];
        node.deleted = true;

        if (!hasChildren(node))
            continue;

        for (int i = 3; i >= 0; --i)
            unvisitedNodesStack.emplace_back(nodeIndex + node.childOffset + i);
    }
}

TerrainTileQuadtree::Visibility TerrainTileQuadtree::calculateNodeVisibility(const Frustum* frustum, const TileTreeNode& node) {
    double normalizedNodeSize = getNormalizedNodeSizeForTreeDepth(node.treeDepth);
    glm::dvec2 normalizedCoord00 = glm::dvec2(node.treePosition) * normalizedNodeSize;
    glm::dvec2 normalizedCoord01 = glm::dvec2(node.treePosition + glm::uvec2(0, 1)) * normalizedNodeSize;
    glm::dvec2 normalizedCoord10 = glm::dvec2(node.treePosition + glm::uvec2(1, 0)) * normalizedNodeSize;
    glm::dvec2 normalizedCoord11 = glm::dvec2(node.treePosition + glm::uvec2(1, 1)) * normalizedNodeSize;

    glm::dvec3 position00((normalizedCoord00.x - 0.5) * m_size.x, 0.0, (normalizedCoord00.y - 0.5) * m_size.y);
    glm::dvec3 position01((normalizedCoord01.x - 0.5) * m_size.x, 0.0, (normalizedCoord01.y - 0.5) * m_size.y);
    glm::dvec3 position10((normalizedCoord10.x - 0.5) * m_size.x, 0.0, (normalizedCoord10.y - 0.5) * m_size.y);
    glm::dvec3 position11((normalizedCoord11.x - 0.5) * m_size.x, 0.0, (normalizedCoord11.y - 0.5) * m_size.y);

    bool c00 = frustum->contains(position00);
    bool c01 = frustum->contains(position01);
    bool c10 = frustum->contains(position10);
    bool c11 = frustum->contains(position11);

    if (c00 && c01 && c10 && c11)
        return Visibility_FullyVisible;

    if (!c00 && !c01 && !c10 && !c11) {
        double r = 0.5 * normalizedNodeSize * glm::max(m_size.x, m_size.y) * glm::root_two<double>();
        BoundingSphere boundingSphere((position00 + position01 + position10 + position11) * 0.25, r);
        testBoundingSpheres.emplace_back(boundingSphere);

        if (!frustum->intersects(boundingSphere))
            return Visibility_NotVisible;
    }

    return Visibility_PartiallyVisible;
}

void TerrainTileQuadtree::cleanupDeletedNodes() {
    PROFILE_SCOPE("TerrainTileQuadtree::cleanupDeletedNodes")
    size_t firstIndex = 0;
    for (; firstIndex < m_nodes.size() && !isDeleted(m_nodes[firstIndex]); ++firstIndex);

    if (firstIndex < m_nodes.size()) {
        for (size_t i = firstIndex; i < m_nodes.size(); ++i) {
            if (!isDeleted(m_nodes[i])) {
                uint32_t offset = (uint32_t)(i - firstIndex);

                assert(m_parentOffsets[i] != UINT32_MAX);

                size_t parentIndex = i - m_parentOffsets[i];
                assert(m_nodes[parentIndex].childOffset < m_nodes.size() - parentIndex);

                if (hasChildren(m_nodes[i])) {
                    for (int j = 0; j < 4; ++j) {
                        size_t childIndex = i + m_nodes[i].childOffset + j;
                        size_t childParentIndex = childIndex - m_parentOffsets[childIndex];
                        assert(childParentIndex + m_nodes[childParentIndex].childOffset == childIndex - j);
                        m_parentOffsets[childIndex] += offset;
                    }
                    m_nodes[i].childOffset += offset;
                }

                // This node is the first child of its parent. The offset from the parent to this node also needs to shift accordingly.
                if (parentIndex + m_nodes[parentIndex].childOffset == i) {
                    assert(m_nodes[parentIndex].childOffset > offset);
                    assert(parentIndex + m_nodes[parentIndex].childOffset == i - m_nodes[i].quadIndex);
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
#else
                m_nodes[firstIndex] = m_nodes[i];
                m_parentOffsets[firstIndex] = m_parentOffsets[i];
#endif

#if _DEBUG
                if (m_parentOffsets[firstIndex] != UINT32_MAX) {
                    size_t testParentIndex = firstIndex - m_parentOffsets[firstIndex];
                    assert(m_nodes[testParentIndex].childOffset <= firstIndex - testParentIndex);
                    assert(testParentIndex + m_nodes[testParentIndex].childOffset == firstIndex - m_nodes[firstIndex].quadIndex);
                }
#endif

                ++firstIndex;
            }
        }

//        LOG_DEBUG("Removing %zu deleted nodes in %.2f msec", (size_t)m_nodes.size() - firstIndex, Performance::milliseconds(t0));

        PROFILE_REGION("Erase deleted nodes")
        m_nodes.erase(m_nodes.begin() + (decltype(m_nodes)::difference_type)firstIndex, m_nodes.end());
        m_parentOffsets.erase(m_parentOffsets.begin() + (decltype(m_parentOffsets)::difference_type)firstIndex, m_parentOffsets.end());


#if _DEBUG
        // SANITY CHECK - every entry in the m_parentOffsets array must make sense.
        for (size_t i = 1; i < m_nodes.size(); ++i) {
            assert(!isDeleted(m_nodes[i]));
            size_t parentIndex = i - m_parentOffsets[i];
            assert(parentIndex + m_nodes[parentIndex].childOffset == i - m_nodes[i].quadIndex);
        }
#endif
    }
}

size_t TerrainTileQuadtree::splitNode(size_t nodeIndex) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::splitNode - node index out of range");

    TileTreeNode& parentNode = m_nodes[nodeIndex];
    assert(parentNode.childOffset == UINT32_MAX && "TerrainTileQuadtree::splitNode - node already split");

    assert(!isDeleted(parentNode));

    if (parentNode.treeDepth >= m_maxQuadtreeDepth)
        return SIZE_MAX; // Cannot split

    if (hasChildren(parentNode))
        return nodeIndex + parentNode.childOffset; // Node already split, return index of first child

    size_t firstChildIndex = m_nodes.size();

//    LOG_DEBUG("Splitting node %zu [%u %u %u] - children at %zu", nodeIndex, parentNode.treePosition.x, parentNode.treePosition.y, parentNode.treeDepth, firstChildIndex);

    parentNode.childOffset = (uint32_t)(firstChildIndex - nodeIndex);

    TileTreeNode childNode{};
    childNode.childOffset = UINT32_MAX;
    childNode.treeDepth = parentNode.treeDepth + 1;

    glm::uvec2 childTreePosition = glm::uvec2(2) * parentNode.treePosition;

    // Note: parentNode reference is invalidated by the addition of new children below.
    for (int i = 0; i < 4; ++i) {
        childNode.quadIndex = (QuadIndex)i;
        childNode.treePosition = childTreePosition + QUAD_OFFSETS[i];
        m_parentOffsets.emplace_back((uint32_t)(m_nodes.size() - nodeIndex)); // Offset to subtract from this child nodes index to reach its parent.
        m_nodes.emplace_back(childNode);
    }

    return firstChildIndex;
}

size_t TerrainTileQuadtree::mergeNode(size_t nodeIndex) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::mergeNode - node index out of range");

    TileTreeNode& node = m_nodes[nodeIndex];
    assert(!isDeleted(node));

    if (!hasChildren(node)) {
        return SIZE_MAX; // Node has no children. Nothing to merge.
    }

    size_t firstChildIndex = nodeIndex + node.childOffset;

//    LOG_DEBUG("Merging node %zu [%u %u %u] - children at %lld", nodeIndex, node.treePosition.x, node.treePosition.y, node.treeDepth, firstChildIndex);

    node.childOffset = UINT32_MAX;

    for (int i = 0; i < 4; ++i) {
        size_t childIndex = firstChildIndex + i;
//        if (hasChildren(m_nodes[childIndex]))
//            mergeNode(childIndex);
        m_nodes[childIndex].deleted = true;
    }

    return firstChildIndex;
}
