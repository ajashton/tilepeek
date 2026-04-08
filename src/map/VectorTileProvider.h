#pragma once

#include "map/TileProvider.h"
#include "mbtiles/MBTilesReader.h"
#include "mvt/MvtTypes.h"

#include <memory>

class VectorTileProvider : public TileProvider {
public:
    VectorTileProvider(std::unique_ptr<MBTilesReader> reader, int minZoom, int maxZoom);

    std::optional<QPixmap> tileAt(int zoom, int x, int y) override;
    std::optional<int> tileSizeAt(int zoom, int x, int y) override;
    int minZoom() const override { return m_minZoom; }
    int maxZoom() const override { return m_maxZoom; }

private:
    QPixmap renderSummaryPixmap(const mvt::Tile& tile) const;

    std::unique_ptr<MBTilesReader> m_reader;
    int m_minZoom;
    int m_maxZoom;
};
