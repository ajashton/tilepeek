#pragma once

#include "map/TileProvider.h"
#include "map/TileSource.h"
#include "mvt/MvtTypes.h"

#include <QColor>
#include <QMutex>
#include <QSet>
#include <QStringList>
#include <memory>
#include <optional>
#include <unordered_map>

class VectorTileProvider : public TileProvider {
public:
    VectorTileProvider(std::unique_ptr<TileSource> source, int minZoom, int maxZoom,
                       const QStringList& layerNames);

    // Thread-safe: can be called from any thread. Source reads and mutable
    // render state are serialized internally; decode and rasterize run lock-free
    // on a thread-local copy.
    std::optional<QImage> tileAt(int zoom, int x, int y) override;
    std::optional<QImage> tileAtSize(int zoom, int x, int y, int size) override;
    std::optional<UnclippedTileResult> tileUnclipped(int zoom, int x, int y, int size) override;
    std::optional<int> tileSizeAt(int zoom, int x, int y) override;
    int minZoom() const override { return m_minZoom; }
    int maxZoom() const override { return m_maxZoom; }

    void setHiddenLayers(const QSet<QString>& hidden);
    void setRenderSize(int size);
    void setDevicePixelRatio(qreal dpr) override;

    // UI-thread only. Uses the same source mutex as the render path.
    std::optional<mvt::Tile> decodeTileAt(int zoom, int x, int y);
    const std::unordered_map<std::string, QColor>& layerColors() const { return m_layerColors; }
    QSet<QString> hiddenLayers() const;

private:
    struct RenderState {
        QSet<QString> hiddenLayers;
        int renderSize;
        qreal dpr;
    };

    std::optional<mvt::Tile> readAndDecode(int zoom, int x, int y);
    RenderState snapshotRenderState() const;

    std::unique_ptr<TileSource> m_source;
    int m_minZoom;
    int m_maxZoom;
    // Immutable after construction; safe for concurrent reads.
    std::unordered_map<std::string, QColor> m_layerColors;

    // m_sourceMutex guards m_source (not concurrent-safe for sqlite) and
    // mutable render state (m_hiddenLayers, m_renderSize, m_dpr). It is held
    // only for the small critical sections — source reads and state snapshots —
    // never while decoding or rasterizing, so multiple workers can rasterize
    // in parallel.
    mutable QMutex m_sourceMutex;
    QSet<QString> m_hiddenLayers;
    int m_renderSize = 256;
    qreal m_dpr = 1.0;
};
