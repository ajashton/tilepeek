#include "map/VectorTileProvider.h"
#include "map/VectorTileRenderer.h"
#include "mvt/MvtDecoder.h"
#include "util/CetColormap.h"
#include "util/GzipUtils.h"

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

std::optional<QPixmap> VectorTileProvider::tileAt(int zoom, int x, int y)
{
    auto blob = m_source->readTile(zoom, x, y);
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

    return VectorTileRenderer::render(*result.tile, m_layerColors, m_hiddenLayers, m_renderSize);
}

std::optional<QPixmap> VectorTileProvider::tileAtSize(int zoom, int x, int y, int size)
{
    auto blob = m_source->readTile(zoom, x, y);
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

    return VectorTileRenderer::render(*result.tile, m_layerColors, m_hiddenLayers, size);
}

std::optional<int> VectorTileProvider::tileSizeAt(int zoom, int x, int y)
{
    return m_source->tileSize(zoom, x, y);
}

void VectorTileProvider::setHiddenLayers(const QSet<QString>& hidden)
{
    m_hiddenLayers = hidden;
}

void VectorTileProvider::setRenderSize(int size)
{
    m_renderSize = size;
}
