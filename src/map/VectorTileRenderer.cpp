#include "map/VectorTileRenderer.h"
#include "mvt/MvtGeometry.h"

#include <QPainter>

QPixmap VectorTileRenderer::render(const mvt::Tile& tile,
                                    const std::unordered_map<std::string, QColor>& layerColors,
                                    const QSet<QString>& hiddenLayers,
                                    int tileSize)
{
    QPixmap pixmap(tileSize, tileSize);
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
