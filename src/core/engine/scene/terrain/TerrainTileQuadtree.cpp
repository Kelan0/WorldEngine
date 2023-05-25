#include "TerrainTileQuadtree.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/util/Logger.h"

// These must match the order of the QuadIndex enum
std::array<glm::uvec2, 4> TerrainTileQuadtree::QUAD_OFFSETS = {
        glm::uvec2(0, 0), // QuadIndex_TopLeft
        glm::uvec2(0, 1), // QuadIndex_BottomLeft
        glm::uvec2(1, 0), // QuadIndex_TopRight
        glm::uvec2(1, 1)  // QuadIndex_BottomRight
};

TerrainTileQuadtree::TerrainTileQuadtree(uint32_t maxQuadtreeDepth, const glm::dvec2& size, double heightScale):
        m_maxQuadtreeDepth(maxQuadtreeDepth),
        m_size(size),
        m_heightScale(heightScale) {

    TileTreeNode rootNode{};
    rootNode.parentOffset = UINT32_MAX;
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

    glm::dvec3 localCameraOrigin = glm::dvec3(glm::inverse(m_transform.getMatrix()) * glm::dvec4(frustum->getOrigin(), 1.0));
    localCameraOrigin.x += m_size.x * 0.5;
    localCameraOrigin.z += m_size.y * 0.5;

//    LOG_DEBUG("Camera origin: [%.2f %.2f %.2f], Local camera origin: [%.2f %.2f %.2f]", frustum->getOrigin().x, frustum->getOrigin().y, frustum->getOrigin().z, localCameraOrigin.x, localCameraOrigin.y, localCameraOrigin.z);

    double maxSize = glm::max(m_size.x, m_size.y);

    double splitThreshold = 3.0;

    m_nodeSplitList.clear();
    m_nodeMergeList.clear();


//    std::stack<size_t> unvisitedNodes;
//    unvisitedNodes.push(0);
//
//    while (!unvisitedNodes.empty()) {
//        const size_t &nodeIndex = unvisitedNodes.top();
//        unvisitedNodes.pop();
//
//        const TileTreeNode &node = m_nodes[nodeIndex];
//        if (isDeleted(node))
//            continue;
//
//        double normalizedNodeSize = getNormalizedNodeSizeForTreeDepth(node.treeDepth);
//        glm::dvec2 normalizedNodeCenterCoord = (glm::dvec2(node.treePosition) + glm::dvec2(0.5)) * normalizedNodeSize;
//        glm::dvec3 nodeCenterPos(normalizedNodeCenterCoord.x * m_size.x, 0.0, normalizedNodeCenterCoord.y * m_size.y);
//
//        glm::dvec3 cameraToNodePosition = nodeCenterPos - localCameraOrigin;
//        double cameraDistanceSq = glm::dot(cameraToNodePosition, cameraToNodePosition);
//
//        double edgeSize = normalizedNodeSize * maxSize;
//        double splitDistance = edgeSize * splitThreshold;
//    }


    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const TileTreeNode& node = m_nodes[i];
        if (isDeleted(node))
            continue;

        double normalizedNodeSize = getNormalizedNodeSizeForTreeDepth(node.treeDepth);
        glm::dvec2 normalizedNodeCenterCoord = (glm::dvec2(node.treePosition) + glm::dvec2(0.5)) * normalizedNodeSize;
        glm::dvec3 nodeCenterPos(normalizedNodeCenterCoord.x * m_size.x, 0.0, normalizedNodeCenterCoord.y * m_size.y);

        glm::dvec3 cameraToNodePosition = nodeCenterPos - localCameraOrigin;
        double cameraDistanceSq = glm::dot(cameraToNodePosition, cameraToNodePosition);

        double edgeSize = normalizedNodeSize * maxSize;
        double splitDistance = edgeSize * splitThreshold;

        if (cameraDistanceSq < splitDistance * splitDistance) {
            // Close enough to split

            if (hasChildren(node) || node.treeDepth >= m_maxQuadtreeDepth)
                continue; // Cannot split

            splitNode(i);

        } else if (cameraDistanceSq > splitDistance * splitDistance) {
            // Far enough to merge

            if (!hasChildren(node))
                continue; // Cannot merge

            mergeNode(i);
        }
    }

//    // Sort split list from near to far. Closest nodes are split first.
//    std::sort(m_nodeSplitList.begin(), m_nodeSplitList.end(), [](const NodeUpdate& lhs, const NodeUpdate& rhs) {
//        return lhs.distance < rhs.distance;
//    });
//
//    // // Sort merge list from far to near. Furthest nodes are merged first. ~~~~~~~~~~
//    std::sort(m_nodeMergeList.begin(), m_nodeMergeList.end(), [](const NodeUpdate& lhs, const NodeUpdate& rhs) {
//        if (lhs.treeDepth != rhs.treeDepth)
//            return lhs.treeDepth < rhs.treeDepth;
//        return lhs.distance > rhs.distance;
//    });
//
//    size_t maxSplits = 8;
//    size_t maxMerges = 1;
//
//    for (auto it = m_nodeSplitList.begin(); it != m_nodeSplitList.end() && maxSplits > 0; ++it, --maxSplits) {
//        assert(it->index < m_nodes.size() && !hasChildren(m_nodes[it->index]));
//        LOG_DEBUG("Splitting node %zu at depth %u", it->index, m_nodes[it->index].treeDepth);
//        splitNode(it->index);
//    }
//    for (auto it = m_nodeMergeList.begin(); it != m_nodeMergeList.end() && maxMerges > 0; ++it, --maxMerges) {
//        assert(it->index < m_nodes.size() && hasChildren(m_nodes[it->index]));
//        LOG_DEBUG("Merging node %zu at depth %u", it->index, m_nodes[it->index].treeDepth);
//        mergeNode(it->index);
//    }
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
    return node.childOffset == 0;
}


size_t TerrainTileQuadtree::getChildIndex(size_t nodeIndex, uint32_t childOffset, QuadIndex quadIndex) {
    return nodeIndex + childOffset + quadIndex;
}

size_t TerrainTileQuadtree::splitNode(size_t nodeIndex) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::splitNode - node index out of range");

    TileTreeNode& parentNode = m_nodes[nodeIndex];
    assert(parentNode.childOffset == UINT32_MAX && "TerrainTileQuadtree::splitNode - node already split");

    size_t firstChildIndex = m_nodes.size();
    parentNode.childOffset = (uint32_t)(firstChildIndex - nodeIndex); // Offset to add to the parent nodes index to reach its first child. All 4 children are consecutive

    TileTreeNode childNode{};
    childNode.childOffset = UINT32_MAX;
    childNode.treeDepth = parentNode.treeDepth + 1;

    glm::uvec2 childTreePosition = glm::uvec2(2) * parentNode.treePosition;

    // Note: parentNode reference is invalidated by the addition of new children below.
    for (int i = 0; i < 4; ++i) {
        childNode.parentOffset = (uint32_t)(m_nodes.size() - nodeIndex); // Offset to subtract from this child nodes index to reach its parent.
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
    node.childOffset = UINT32_MAX;

    for (int i = 0; i < 4; ++i) {
        size_t childIndex = firstChildIndex + i;
//        if (hasChildren(m_nodes[childIndex]))
//            mergeNode(childIndex);
        m_nodes[childIndex].childOffset = 0; // Child points to self, indicates deleted node.
    }


//    m_nodes.erase(m_nodes.begin() + firstChildIndex, m_nodes.begin() + firstChildIndex + 4);
//
//    for (size_t i = 0; i < m_nodes.size() && i < firstChildIndex; ++i) {
//        if (hasChildren(m_nodes[i]) && i + m_nodes[i].childOffset >= firstChildIndex) {
//            assert(m_nodes[i].childOffset > 4); // childOffset cannot be 0 (child cannot be self)
//            m_nodes[i].childOffset -= 4;
//        }
//    }

    return (size_t)firstChildIndex;
}
