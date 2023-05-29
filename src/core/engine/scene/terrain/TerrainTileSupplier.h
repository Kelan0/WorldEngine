
#ifndef WORLDENGINE_TERRAINTILESUPPLIER_H
#define WORLDENGINE_TERRAINTILESUPPLIER_H

#include "core/core.h"

class Image2D;
class ImageView;

class TerrainTileSupplier;

class TerrainTileData {
    friend class TerrainTileSupplier;

private:
    TerrainTileData();

    ~TerrainTileData();
public:

private:
    TerrainTileSupplier* m_tileSupplier;
    uint32_t m_tileTextureIndex;
    glm::dvec2 m_tileOffset;
    glm::dvec2 m_tileSize;
    double m_minHeight;
    double m_maxHeight;
    float* m_heightData;
};




class TerrainTileSupplier {
public:
    TerrainTileSupplier();

    virtual ~TerrainTileSupplier();

    virtual const std::vector<ImageView*>& getLoadedTileImageViews() const = 0;

private:

};




class HeightmapTerrainTileSupplier : public TerrainTileSupplier {
public:
    HeightmapTerrainTileSupplier(const std::shared_ptr<Image2D>& heightmapImage);

    virtual ~HeightmapTerrainTileSupplier() override;

    virtual const std::vector<ImageView*>& getLoadedTileImageViews() const override;

    const std::shared_ptr<Image2D>& getHeightmapImage() const;

    const std::shared_ptr<ImageView>& getHeightmapImageView() const;

private:
    std::shared_ptr<Image2D> m_heightmapImage;
    std::shared_ptr<ImageView> m_heightmapImageView;
    std::vector<ImageView*> m_loadedTileImageViews;
};


#endif //WORLDENGINE_TERRAINTILESUPPLIER_H
