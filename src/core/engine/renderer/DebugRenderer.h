#ifndef WORLDENGINE_DEBUGRENDERER_H
#define WORLDENGINE_DEBUGRENDERER_H

#include "core/core.h"
#include "core/engine/geometry/MeshData.h"


class DebugRenderer {
public:
    enum PolygonMode {
        Mode_Triangles = 0,
        Mode_Lines = 1,
        Mode_LineLoop = 2,
        Mode_Points = 3,
    };

    class DisplayList {
        friend class DebugRenderer;
    private:
        DisplayList(const size_t& id, const PolygonMode& polygonMode);

        size_t m_id;
        PolygonMode m_polygonMode;
        std::vector<MeshData::Vertex> m_vertices;
        std::vector<MeshData::Index> m_indices;
        glm::mat4 m_matrix;
    };

public:
    DebugRenderer();

    ~DebugRenderer();

    void init();

    void render(double dt);

    DisplayList* createDisplayList(const PolygonMode& polygonMode);

    void destroyDisplayList(DisplayList*& displayList);

private:
    static size_t s_nextDisplayListId;
};


#endif //WORLDENGINE_DEBUGRENDERER_H
