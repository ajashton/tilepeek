#include "map/VectorTileRenderer.h"
#include "mvt/MvtGeometry.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

QPixmap VectorTileRenderer::render(const mvt::Tile& tile,
                                    const std::unordered_map<std::string, QColor>& layerColors,
                                    const QSet<QString>& hiddenLayers,
                                    int tileSize,
                                    qreal dpr)
{
    int phys = static_cast<int>(std::lround(tileSize * dpr));
    QPixmap pixmap(phys, phys);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(QColor("#000000"));

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    for (const auto& layer : tile.layers) {
        QString layerName = QString::fromStdString(layer.name);
        if (hiddenLayers.contains(layerName))
            continue;

        QColor baseColor(100, 100, 100);
        auto it = layerColors.find(layer.name);
        if (it != layerColors.end())
            baseColor = it->second;

        double extent = layer.extent;

        for (const auto& feature : layer.features) {
            switch (feature.type) {
            case mvt::GeomType::Polygon: {
                auto path = mvt::decodeGeometry(feature, extent, tileSize);
                QColor fillColor = baseColor;
                fillColor.setAlpha(25); // ~10%
                QColor strokeColor = baseColor;
                strokeColor.setAlpha(127); // ~50%
                painter.setBrush(fillColor);
                painter.setPen(QPen(strokeColor, 1.0));
                painter.drawPath(path);
                break;
            }
            case mvt::GeomType::LineString: {
                auto path = mvt::decodeGeometry(feature, extent, tileSize);
                QColor strokeColor = baseColor;
                strokeColor.setAlpha(127);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(strokeColor, 1.0));
                painter.drawPath(path);
                break;
            }
            case mvt::GeomType::Point: {
                auto points = mvt::decodePoints(feature, extent, tileSize);
                QColor dotColor = baseColor;
                dotColor.setAlpha(127);
                painter.setPen(Qt::NoPen);
                painter.setBrush(dotColor);
                for (const auto& pt : points)
                    painter.drawEllipse(pt, 3.0, 3.0);
                break;
            }
            default:
                break;
            }
        }
    }

    return pixmap;
}

static void drawFeatures(QPainter& painter, const mvt::Tile& tile,
                          const std::unordered_map<std::string, QColor>& layerColors,
                          const QSet<QString>& hiddenLayers,
                          int tileSize)
{
    for (const auto& layer : tile.layers) {
        QString layerName = QString::fromStdString(layer.name);
        if (hiddenLayers.contains(layerName))
            continue;

        QColor baseColor(100, 100, 100);
        auto it = layerColors.find(layer.name);
        if (it != layerColors.end())
            baseColor = it->second;

        double extent = layer.extent;

        for (const auto& feature : layer.features) {
            switch (feature.type) {
            case mvt::GeomType::Polygon: {
                auto path = mvt::decodeGeometry(feature, extent, tileSize);
                QColor fillColor = baseColor;
                fillColor.setAlpha(25);
                QColor strokeColor = baseColor;
                strokeColor.setAlpha(127);
                painter.setBrush(fillColor);
                painter.setPen(QPen(strokeColor, 1.0));
                painter.drawPath(path);
                break;
            }
            case mvt::GeomType::LineString: {
                auto path = mvt::decodeGeometry(feature, extent, tileSize);
                QColor strokeColor = baseColor;
                strokeColor.setAlpha(127);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(strokeColor, 1.0));
                painter.drawPath(path);
                break;
            }
            case mvt::GeomType::Point: {
                auto points = mvt::decodePoints(feature, extent, tileSize);
                QColor dotColor = baseColor;
                dotColor.setAlpha(127);
                painter.setPen(Qt::NoPen);
                painter.setBrush(dotColor);
                for (const auto& pt : points)
                    painter.drawEllipse(pt, 3.0, 3.0);
                break;
            }
            default:
                break;
            }
        }
    }
}

UnclippedTileResult VectorTileRenderer::renderUnclipped(
    const mvt::Tile& tile,
    const std::unordered_map<std::string, QColor>& layerColors,
    const QSet<QString>& hiddenLayers,
    int tileSize,
    qreal dpr)
{
    // Scan all geometry to find the actual extent of buffer data
    double minCoord = 0.0, maxCoord = 0.0;
    for (const auto& layer : tile.layers) {
        double extent = layer.extent;
        for (const auto& feature : layer.features) {
            auto bounds = mvt::geometryBounds(feature);
            if (bounds.isNull())
                continue;
            minCoord = std::min(minCoord, std::min(bounds.left() / extent, bounds.top() / extent));
            maxCoord = std::max(maxCoord, std::max(bounds.right() / extent, bounds.bottom() / extent));
        }
    }

    // bufferRatio: how far geometry extends beyond [0, 1] as a fraction of extent
    double negBuffer = std::max(0.0, -minCoord);
    double posBuffer = std::max(0.0, maxCoord - 1.0);
    double bufferRatio = std::max(negBuffer, posBuffer);

    // Ensure a minimum buffer so the tile boundary is visible
    bufferRatio = std::max(bufferRatio, 0.02);

    double totalRatio = 1.0 + 2.0 * bufferRatio;
    int pixmapSize = static_cast<int>(std::ceil(tileSize * totalRatio));
    int bufferPixels = static_cast<int>(std::round(tileSize * bufferRatio));

    int physPixmapSize = static_cast<int>(std::lround(pixmapSize * dpr));
    QPixmap pixmap(physPixmapSize, physPixmapSize);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(QColor("#000000"));

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(bufferPixels, bufferPixels);

    // Draw all geometry using the base tileSize (geometry at [0,extent] maps to [0,tileSize])
    drawFeatures(painter, tile, layerColors, hiddenLayers, tileSize);

    return {pixmap, bufferRatio};
}
