#pragma once

#include "map/TileProvider.h"
#include "map/TileSource.h"

#include <QColor>
#include <QSet>
#include <QStringList>
#include <memory>
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

private:
    std::unique_ptr<TileSource> m_source;
    int m_minZoom;
    int m_maxZoom;
    std::unordered_map<std::string, QColor> m_layerColors;
    QSet<QString> m_hiddenLayers;
    int m_renderSize = 256;
};
