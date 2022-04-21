//
// Created by Kelan on 20/04/2022.
//

#include "DebugRenderer.h"

size_t DebugRenderer::s_nextDisplayListId = 0;

DebugRenderer::DisplayList::DisplayList(const size_t& id, const DebugRenderer::PolygonMode &polygonMode):
    m_id(id),
    m_polygonMode(polygonMode),
    m_matrix(1.0) {
}


DebugRenderer::DebugRenderer() {

}

DebugRenderer::~DebugRenderer() {

}

void DebugRenderer::init() {

}

void DebugRenderer::render(double dt) {

}

DebugRenderer::DisplayList* DebugRenderer::createDisplayList(const DebugRenderer::PolygonMode &polygonMode) {
    DisplayList* displayList = new DisplayList(s_nextDisplayListId++, polygonMode);
    // TODO: record the list internally
    return displayList;
}

void DebugRenderer::destroyDisplayList(DebugRenderer::DisplayList*& displayList) {
    delete displayList;
    displayList = nullptr;
}
