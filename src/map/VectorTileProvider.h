#pragma once

#include "map/TileProvider.h"
#include "map/TileSource.h"
#include "mvt/MvtTypes.h"

#include <QColor>
#include <QSet>
#include <QStringList>
#include <memory>
#include <optional>
#include <unordered_map>

class VectorTileProvider : public TileProvider {
public:
    VectorTileProvider(std::unique_ptr<TileSource> source, int minZoom, int maxZoom,
                       const QStringList& layerNames);

    std::optional<QPixmap> tileAt(int zoom, int x, int y) override;
    std::optional<QPixmap> tileAtSize(int zoom, int x, int y, int size) override;
    std::optional<UnclippedTileResult> tileUnclipped(int zoom, int x, int y, int size) override;
    std::optional<int> tileSizeAt(int zoom, int x, int y) override;
    int minZoom() const override { return m_minZoom; }
    int maxZoom() const override { return m_maxZoom; }

    void setHiddenLayers(const QSet<QString>& hidden);
    void setRenderSize(int size);

    std::optional<mvt::Tile> decodeTileAt(int zoom, int x, int y);
    const std::unordered_map<std::string, QColor>& layerColors() const { return m_layerColors; }
    const QSet<QString>& hiddenLayers() const { return m_hiddenLayers; }

private:
    std::optional<mvt::Tile> readAndDecode(int zoom, int x, int y);

    std::unique_ptr<TileSource> m_source;
    int m_minZoom;
    int m_maxZoom;
    std::unordered_map<std::string, QColor> m_layerColors;
    QSet<QString> m_hiddenLayers;
    int m_renderSize = 256;
};
