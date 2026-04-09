#include "map/VectorTileProvider.h"
#include "map/VectorTileRenderer.h"
#include "mvt/MvtDecoder.h"
#include "util/CetColormap.h"
#include "util/GzipUtils.h"
#include "util/TileCoords.h"

VectorTileProvider::VectorTileProvider(std::unique_ptr<MBTilesReader> reader,
                                       int minZoom, int maxZoom,
                                       const QStringList& layerNames)
    : m_reader(std::move(reader))
    , m_minZoom(minZoom)
    , m_maxZoom(maxZoom)
{
    auto colors = CetColormap::pickColors(layerNames.size());
    for (int i = 0; i < layerNames.size(); ++i)
        m_layerColors[layerNames[i].toStdString()] = colors[i];
}

std::optional<QPixmap> VectorTileProvider::tileAt(int zoom, int x, int y)
{
    int tmsRow = TileCoords::xyzToTms(zoom, y);
    auto blob = m_reader->readTileData(zoom, x, tmsRow);
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

std::optional<int> VectorTileProvider::tileSizeAt(int zoom, int x, int y)
{
    int tmsRow = TileCoords::xyzToTms(zoom, y);
    auto blob = m_reader->readTileData(zoom, x, tmsRow);
    if (!blob)
        return std::nullopt;
    return static_cast<int>(blob->size());
}

void VectorTileProvider::setHiddenLayers(const QSet<QString>& hidden)
{
    m_hiddenLayers = hidden;
}

void VectorTileProvider::setRenderSize(int size)
{
    m_renderSize = size;
}
