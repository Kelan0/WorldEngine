#include "TerrainTileQuadtree.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/util/Logger.h"
#include "core/util/Profiler.h"

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
        m_heightScale(heightScale) {

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
    auto t0 = Performance::now();

    glm::dvec3 localCameraOrigin = glm::dvec3(glm::inverse(m_transform.getMatrix()) * glm::dvec4(frustum->getOrigin(), 1.0));
    localCameraOrigin.x += m_size.x * 0.5;
    localCameraOrigin.z += m_size.y * 0.5;

//    LOG_DEBUG("Camera origin: [%.2f %.2f %.2f], Local camera origin: [%.2f %.2f %.2f]", frustum->getOrigin().x, frustum->getOrigin().y, frustum->getOrigin().z, localCameraOrigin.x, localCameraOrigin.y, localCameraOrigin.z);

    double maxSize = glm::max(m_size.x, m_size.y);

    double splitThreshold = 3.0;

    std::vector<uint32_t> parentOffsets;
    parentOffsets.resize(m_nodes.size(), UINT32_MAX);

    std::vector<size_t> deletedNodeIndices;
    std::stack<size_t> unvisitedNodes;
    unvisitedNodes.push(0);

    PROFILE_REGION("Split and merge nodes")


    while (!unvisitedNodes.empty()) {
        size_t nodeIndex = unvisitedNodes.top();
        unvisitedNodes.pop();

        assert(nodeIndex < m_nodes.size());

        const TileTreeNode& node = m_nodes[nodeIndex];
        if (isDeleted(node)) {
            deletedNodeIndices.emplace_back(nodeIndex);
            continue;
        }

        double normalizedNodeSize = getNormalizedNodeSizeForTreeDepth(node.treeDepth);
        glm::dvec2 normalizedNodeCenterCoord = (glm::dvec2(node.treePosition) + glm::dvec2(0.5)) * normalizedNodeSize;
        glm::dvec3 nodeCenterPos(normalizedNodeCenterCoord.x * m_size.x, 0.0, normalizedNodeCenterCoord.y * m_size.y);

        glm::dvec3 cameraToNodePosition = nodeCenterPos - localCameraOrigin;
        double cameraDistanceSq = glm::dot(cameraToNodePosition, cameraToNodePosition);

        double edgeSize = normalizedNodeSize * maxSize;
        double splitDistance = edgeSize * splitThreshold;

        if (cameraDistanceSq < splitDistance * splitDistance) {
            // Close enough to split

            if (!hasChildren(node) && node.treeDepth < m_maxQuadtreeDepth) {
                // NOTE: splitNode here will grow the list, and potentially invalidate the node reference above.
                size_t firstChildIndex = splitNode(nodeIndex);
                for (int i = 0; i < 4; ++i)
                    parentOffsets.emplace_back((uint32_t)((firstChildIndex + i) - nodeIndex));
            }

        } else if (cameraDistanceSq > splitDistance * splitDistance) {
            // Far enough to merge

            if (hasChildren(node)) {
                // NOTE: mergeNode is okay while iterating, it will not affect the size of the array or move anything around.
                size_t deletedChildIndex = mergeNode(nodeIndex);
                for (int i = 0; i < 4; ++i, ++deletedChildIndex)
                    deletedNodeIndices.emplace_back(deletedChildIndex);
            }
        }

        // node reference above may have been invalidated here, so we must index the nodes array directly
        if (hasChildren(m_nodes[nodeIndex])) {
            size_t firstChildIndex = nodeIndex + m_nodes[nodeIndex].childOffset;
            assert(firstChildIndex <= m_nodes.size() - 4);
            for (int i = 3; i >= 0; --i) {
                size_t childIndex = firstChildIndex + i;
                unvisitedNodes.push(childIndex);
                parentOffsets[childIndex] = (uint32_t)(childIndex - nodeIndex);
            }
        }
    }

    PROFILE_REGION("Mark all deleted subtrees")

    for (const size_t& nodeIndex : deletedNodeIndices)
        unvisitedNodes.push(nodeIndex);

    while (!unvisitedNodes.empty()) {
        size_t nodeIndex = unvisitedNodes.top();
        unvisitedNodes.pop();

        TileTreeNode& node = m_nodes[nodeIndex];
        node.deleted = true;

        if (!hasChildren(node))
            continue;

        for (int i = 3; i >= 0; --i) {
            unvisitedNodes.push(nodeIndex + node.childOffset + i);
        }
    }

    PROFILE_REGION("Cleanup deleted nodes")
    size_t firstIndex = 0;
    for (; firstIndex < m_nodes.size() && !isDeleted(m_nodes[firstIndex]); ++firstIndex);

    if (firstIndex < m_nodes.size()) {
        for (size_t i = firstIndex; i < m_nodes.size(); ++i) {
            if (!isDeleted(m_nodes[i])) {
                uint32_t offset = (uint32_t)(i - firstIndex);

                assert(parentOffsets[i] != UINT32_MAX);

                size_t parentIndex = i - parentOffsets[i];
                assert(m_nodes[parentIndex].childOffset < m_nodes.size() - parentIndex);

                if (hasChildren(m_nodes[i])) {
                    for (int j = 0; j < 4; ++j) {
                        size_t childIndex = i + m_nodes[i].childOffset + j;
                        size_t childParentIndex = childIndex - parentOffsets[childIndex];
                        assert(childParentIndex + m_nodes[childParentIndex].childOffset == childIndex - j);
                        parentOffsets[childIndex] += offset;
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
                    parentOffsets[i] -= offset;
                    assert(parentOffsets[i] <= firstIndex);
                }

                // element at index i moves to firstIndex
#if 0
                std::swap(m_nodes[firstIndex], m_nodes[i]);
                std::swap(parentOffsets[firstIndex], parentOffsets[i]);
#else
                m_nodes[firstIndex] = m_nodes[i];
                parentOffsets[firstIndex] = parentOffsets[i];
#endif

#if _DEBUG
                if (parentOffsets[firstIndex] != UINT32_MAX) {
                    size_t testParentIndex = firstIndex - parentOffsets[firstIndex];
                    assert(m_nodes[testParentIndex].childOffset <= firstIndex - testParentIndex);
                    assert(testParentIndex + m_nodes[testParentIndex].childOffset == firstIndex - m_nodes[firstIndex].quadIndex);
                }
#endif

                ++firstIndex;
            }
        }

        LOG_DEBUG("Removing %zu deleted nodes in %.2f msec", (size_t)m_nodes.size() - firstIndex, Performance::milliseconds(t0));

        PROFILE_REGION("Erase deleted nodes")
        m_nodes.erase(m_nodes.begin() + (decltype(m_nodes)::difference_type)firstIndex, m_nodes.end());
        parentOffsets.erase(parentOffsets.begin() + (decltype(parentOffsets)::difference_type)firstIndex, parentOffsets.end());


#if _DEBUG
        // SANITY CHECK - every entry in the parentOffsets array must make sense.
        for (size_t i = 1; i < m_nodes.size(); ++i) {
            assert(!isDeleted(m_nodes[i]));
            size_t parentIndex = i - parentOffsets[i];
            assert(parentIndex + m_nodes[parentIndex].childOffset == i - m_nodes[i].quadIndex);
        }
#endif
    }
}

const std::vector<TerrainTileQuadtree::TileTreeNode>& TerrainTileQuadtree::getNodes() {
    return m_nodes;
}

glm::dvec2 TerrainTileQuadtree::getNormalizedNodeCoordinate(const glm::dvec2& treePosition, uint8_t treeDepth) {
    return treePosition * getNormalizedNodeSizeForTreeDepth(treeDepth);
}

double TerrainTileQuadtree::getNormalizedNodeSizeForTreeDepth(uint8_t treeDepth) {
    return 1.0 / (double)(1 << treeDepth);
}

glm::dvec3 TerrainTileQuadtree::getNodePosition(const glm::dvec2& normalizedNodeCoordinate) {
    return Transform::apply(m_transform, glm::dvec3((normalizedNodeCoordinate - glm::dvec2(0.5)) * m_size, 0.0F));
}

glm::dvec2 TerrainTileQuadtree::getNodeSize(uint8_t treeDepth) {
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


size_t TerrainTileQuadtree::splitNode(size_t nodeIndex) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::splitNode - node index out of range");

    TileTreeNode& parentNode = m_nodes[nodeIndex];
    assert(parentNode.childOffset == UINT32_MAX && "TerrainTileQuadtree::splitNode - node already split");


    size_t firstChildIndex = m_nodes.size();

//    LOG_DEBUG("Splitting node %zu [%u %u %u] - children at %zu", nodeIndex, parentNode.treePosition.x, parentNode.treePosition.y, parentNode.treeDepth, firstChildIndex);

    parentNode.childOffset = (uint32_t)(firstChildIndex - nodeIndex);

    TileTreeNode childNode{};
    childNode.childOffset = UINT32_MAX;
    childNode.treeDepth = parentNode.treeDepth + 1;

    glm::uvec2 childTreePosition = glm::uvec2(2) * parentNode.treePosition;

    // Note: parentNode reference is invalidated by the addition of new children below.
    for (int i = 0; i < 4; ++i) {
//        childNode.parentOffset = (uint32_t)(m_nodes.size() - nodeIndex); // Offset to subtract from this child nodes index to reach its parent.
        childNode.quadIndex = (QuadIndex)i;
        childNode.treePosition = childTreePosition + QUAD_OFFSETS[childNode.quadIndex];
        m_nodes.emplace_back(childNode);
    }

    return firstChildIndex;
}

size_t TerrainTileQuadtree::mergeNode(size_t nodeIndex) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::mergeNode - node index out of range");

    TileTreeNode& node = m_nodes[nodeIndex];
    if (isDeleted(node) || !hasChildren(node)) {
        return SIZE_MAX; // Node has no children. Nothing to merge.
    }

    int64_t firstChildIndex = (int64_t)(nodeIndex + node.childOffset);

//    LOG_DEBUG("Merging node %zu [%u %u %u] - children at %lld", nodeIndex, node.treePosition.x, node.treePosition.y, node.treeDepth, firstChildIndex);

    node.childOffset = UINT32_MAX;

    for (int i = 0; i < 4; ++i) {
        size_t childIndex = firstChildIndex + i;
//        if (hasChildren(m_nodes[childIndex]))
//            mergeNode(childIndex);
        m_nodes[childIndex].deleted = true;
    }

    return (size_t)firstChildIndex;
}
