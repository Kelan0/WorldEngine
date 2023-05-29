
#include "core/engine/scene/terrain/TerrainTileSupplier.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"


TerrainTileSupplier::TerrainTileSupplier() {

};

TerrainTileSupplier::~TerrainTileSupplier() {

}

HeightmapTerrainTileSupplier::HeightmapTerrainTileSupplier(const std::shared_ptr<Image2D>& heightmapImage) :
        TerrainTileSupplier(),
        m_heightmapImage(heightmapImage),
        m_heightmapImageView(nullptr) {

    if (m_heightmapImage) {
        ImageViewConfiguration heightmapImageViewConfig{};
        heightmapImageViewConfig.device = heightmapImage->getDevice();
        heightmapImageViewConfig.format = heightmapImage->getFormat();
        heightmapImageViewConfig.setImage(heightmapImage.get());
        m_heightmapImageView = std::shared_ptr<ImageView>(ImageView::create(heightmapImageViewConfig, heightmapImage->getName() + "-ImageView"));

        m_loadedTileImageViews.emplace_back(m_heightmapImageView.get());
    }
}

HeightmapTerrainTileSupplier::~HeightmapTerrainTileSupplier() {

}

const std::vector<ImageView*>& HeightmapTerrainTileSupplier::getLoadedTileImageViews() const {
    return m_loadedTileImageViews;
}

const std::shared_ptr<Image2D>& HeightmapTerrainTileSupplier::getHeightmapImage() const {
    return m_heightmapImage;
}

const std::shared_ptr<ImageView>& HeightmapTerrainTileSupplier::getHeightmapImageView() const {
    return m_heightmapImageView;
}
