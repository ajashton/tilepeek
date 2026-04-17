#include "map/VectorTileProvider.h"
#include "map/VectorTileRenderer.h"
#include "mvt/MvtDecoder.h"
#include "util/CetColormap.h"
#include "util/GzipUtils.h"

#include <QMutexLocker>

VectorTileProvider::VectorTileProvider(std::unique_ptr<TileSource> source,
                                       int minZoom, int maxZoom,
                                       const QStringList& layerNames)
    : m_source(std::move(source))
    , m_minZoom(minZoom)
    , m_maxZoom(maxZoom)
{
    auto colors = CetColormap::pickColors(layerNames.size());
    for (int i = 0; i < layerNames.size(); ++i)
        m_layerColors[layerNames[i].toStdString()] = colors[i];
}

std::optional<mvt::Tile> VectorTileProvider::readAndDecode(int zoom, int x, int y)
{
    std::optional<QByteArray> blob;
    {
        QMutexLocker lock(&m_sourceMutex);
        blob = m_source->readTile(zoom, x, y);
    }
    if (!blob)
        return std::nullopt;

    QByteArray data = *blob;
    if (GzipUtils::isGzipCompressed(data)) {
        auto decompressed = GzipUtils::decompress(data);
        if (!decompressed)
            return std::nullopt;
        data = *decompressed;
    }

    auto result = mvt::decodeTile(data);
    if (!result.tile)
        return std::nullopt;

    return std::move(*result.tile);
}

VectorTileProvider::RenderState VectorTileProvider::snapshotRenderState() const
{
    QMutexLocker lock(&m_sourceMutex);
    return RenderState{m_hiddenLayers, m_renderSize, m_dpr};
}

std::optional<QImage> VectorTileProvider::tileAt(int zoom, int x, int y)
{
    auto tile = readAndDecode(zoom, x, y);
    if (!tile)
        return std::nullopt;
    auto state = snapshotRenderState();
    return VectorTileRenderer::render(*tile, m_layerColors, state.hiddenLayers,
                                      state.renderSize, state.dpr);
}

std::optional<QImage> VectorTileProvider::tileAtSize(int zoom, int x, int y, int size)
{
    auto tile = readAndDecode(zoom, x, y);
    if (!tile)
        return std::nullopt;
    auto state = snapshotRenderState();
    return VectorTileRenderer::render(*tile, m_layerColors, state.hiddenLayers, size, state.dpr);
}

std::optional<UnclippedTileResult> VectorTileProvider::tileUnclipped(int zoom, int x, int y, int size)
{
    auto tile = readAndDecode(zoom, x, y);
    if (!tile)
        return std::nullopt;
    auto state = snapshotRenderState();
    return VectorTileRenderer::renderUnclipped(*tile, m_layerColors, state.hiddenLayers,
                                               size, state.dpr);
}

std::optional<int> VectorTileProvider::tileSizeAt(int zoom, int x, int y)
{
    QMutexLocker lock(&m_sourceMutex);
    return m_source->tileSize(zoom, x, y);
}

void VectorTileProvider::setHiddenLayers(const QSet<QString>& hidden)
{
    QMutexLocker lock(&m_sourceMutex);
    m_hiddenLayers = hidden;
}

QSet<QString> VectorTileProvider::hiddenLayers() const
{
    QMutexLocker lock(&m_sourceMutex);
    return m_hiddenLayers;
}

void VectorTileProvider::setRenderSize(int size)
{
    QMutexLocker lock(&m_sourceMutex);
    m_renderSize = size;
}

void VectorTileProvider::setDevicePixelRatio(qreal dpr)
{
    QMutexLocker lock(&m_sourceMutex);
    m_dpr = dpr;
}

std::optional<mvt::Tile> VectorTileProvider::decodeTileAt(int zoom, int x, int y)
{
    return readAndDecode(zoom, x, y);
}
