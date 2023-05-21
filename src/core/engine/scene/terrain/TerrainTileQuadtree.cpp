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

    m_leafNodeQuads.emplace_back(getNodeQuad(0));
}

TerrainTileQuadtree::~TerrainTileQuadtree() {

}

void TerrainTileQuadtree::update(const Frustum* frustum) {
    glm::dvec2 normalizedNodeCoord = getNormalizedNodeCoordinate(0, glm::dvec2(0.5, 0.5));
    glm::dvec3 position = getNodePosition(normalizedNodeCoord);

//    double distance = glm::distance(position, frustum->getOrigin());
}

const std::vector<TerrainTileQuadtree::NodeQuad>& TerrainTileQuadtree::getLeafNodeQuads() {
    return m_leafNodeQuads;
}

glm::dvec2 TerrainTileQuadtree::getNormalizedNodeCoordinate(size_t nodeIndex, const glm::dvec2& offset) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::getNormalizedNodeCoordinate - node index out of range");

    const TileTreeNode& node = m_nodes[nodeIndex];
    return (glm::dvec2(node.treePosition) + offset) * getNormalizedNodeSizeForTreeDepth(node.treeDepth);
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

TerrainTileQuadtree::NodeQuad TerrainTileQuadtree::getNodeQuad(size_t nodeIndex) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::getNodeQuad - node index out of range");

    const TileTreeNode& node = m_nodes[nodeIndex];

    NodeQuad nodeQuad{};
    nodeQuad.treeDepth = node.treeDepth;
    nodeQuad.treePosition = node.treePosition;
    nodeQuad.normalizedCoord = getNormalizedNodeCoordinate(nodeIndex, glm::dvec2(0.0));
    nodeQuad.normalizedSize = getNormalizedNodeSizeForTreeDepth(node.treeDepth);

    return nodeQuad;
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

void TerrainTileQuadtree::splitNode(size_t nodeIndex) {
    assert(nodeIndex < m_nodes.size() && "TerrainTileQuadtree::splitNode - node index out of range");

    TileTreeNode& parentNode = m_nodes[nodeIndex];
    assert(parentNode.childOffset == UINT32_MAX && "TerrainTileQuadtree::splitNode - node already split");

    size_t firstChildIndex = m_nodes.size();
    parentNode.childOffset = (uint32_t)(firstChildIndex - nodeIndex); // Offset to add to the parent nodes index to reach its first child. All 4 children are consecutive

    TileTreeNode childNode{};
    childNode.childOffset = UINT32_MAX;
    childNode.treeDepth = parentNode.treeDepth + 1;

    for (int i = 0; i < 4; ++i) {
        childNode.parentOffset = (uint32_t)(m_nodes.size() - nodeIndex); // Offset to subtract from this child nodes index to reach its parent.
        childNode.quadIndex = (QuadIndex)i;
        childNode.treePosition = glm::uvec2(2) * parentNode.treePosition + QUAD_OFFSETS[childNode.quadIndex];
        m_nodes.emplace_back(childNode);
    }
}

void TerrainTileQuadtree::mergeNode(size_t nodeIndex) {

}
