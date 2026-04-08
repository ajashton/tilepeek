#include "map/VectorTileProvider.h"
#include "mvt/MvtDecoder.h"
#include "util/GzipUtils.h"
#include "util/TileCoords.h"

#include <QPainter>

VectorTileProvider::VectorTileProvider(std::unique_ptr<MBTilesReader> reader,
                                       int minZoom, int maxZoom)
    : m_reader(std::move(reader))
    , m_minZoom(minZoom)
    , m_maxZoom(maxZoom)
{
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

    return renderSummaryPixmap(*result.tile);
}

std::optional<int> VectorTileProvider::tileSizeAt(int zoom, int x, int y)
{
    int tmsRow = TileCoords::xyzToTms(zoom, y);
    auto blob = m_reader->readTileData(zoom, x, tmsRow);
    if (!blob)
        return std::nullopt;
    return static_cast<int>(blob->size());
}

QPixmap VectorTileProvider::renderSummaryPixmap(const mvt::Tile& tile) const
{
    QPixmap pixmap(256, 256);
    pixmap.fill(QColor("#e8edf2"));

    QPainter painter(&pixmap);
    QFont mono("monospace", 9);
    mono.setStyleHint(QFont::Monospace);
    painter.setFont(mono);
    painter.setPen(QColor("#334"));

    QFontMetrics fm(mono);
    int lineHeight = fm.height();
    int y = 8 + fm.ascent();

    for (const auto& layer : tile.layers) {
        QString line = QString("%1: %2")
                           .arg(QString::fromStdString(layer.name))
                           .arg(layer.features.size());
        painter.drawText(8, y, line);
        y += lineHeight;

        if (y > 248)
            break;
    }

    if (tile.layers.empty()) {
        painter.setPen(QColor("#999"));
        painter.drawText(8, y, "(empty tile)");
    }

    return pixmap;
}
