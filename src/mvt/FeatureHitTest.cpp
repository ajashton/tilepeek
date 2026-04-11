#include "mvt/FeatureHitTest.h"
#include "mvt/MvtGeometry.h"

#include <QPainterPathStroker>
#include <cmath>

namespace mvt {

QString valueToString(const Value& value)
{
    return std::visit(
        [](const auto& v) -> QString {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
                return QString::fromStdString(v);
            else if constexpr (std::is_same_v<T, bool>)
                return v ? QStringLiteral("true") : QStringLiteral("false");
            else if constexpr (std::is_same_v<T, float>)
                return QString::number(static_cast<double>(v));
            else if constexpr (std::is_same_v<T, double>)
                return QString::number(v);
            else if constexpr (std::is_same_v<T, int64_t>)
                return QString::number(v);
            else if constexpr (std::is_same_v<T, uint64_t>)
                return QString::number(v);
            else
                return {};
        },
        value);
}

static QVector<std::pair<QString, QString>> decodeProperties(const Feature& feature,
                                                              const Layer& layer)
{
    QVector<std::pair<QString, QString>> props;
    for (size_t i = 0; i + 1 < feature.tags.size(); i += 2) {
        uint32_t keyIdx = feature.tags[i];
        uint32_t valIdx = feature.tags[i + 1];
        if (keyIdx < layer.keys.size() && valIdx < layer.values.size()) {
            props.append({QString::fromStdString(layer.keys[keyIdx]),
                          valueToString(layer.values[valIdx])});
        }
    }
    return props;
}

static bool hitTestFeature(const Feature& feature, double extent, double tileSize,
                           QPointF pos, double hitRadiusPx)
{
    switch (feature.type) {
    case GeomType::Polygon: {
        auto path = decodeGeometry(feature, extent, tileSize);
        if (path.contains(pos))
            return true;
        QPainterPathStroker stroker;
        stroker.setWidth(hitRadiusPx * 2.0);
        return stroker.createStroke(path).contains(pos);
    }
    case GeomType::LineString: {
        auto path = decodeGeometry(feature, extent, tileSize);
        QPainterPathStroker stroker;
        stroker.setWidth(hitRadiusPx * 2.0);
        return stroker.createStroke(path).contains(pos);
    }
    case GeomType::Point: {
        auto points = decodePoints(feature, extent, tileSize);
        double threshold = std::max(3.0, hitRadiusPx);
        double thresholdSq = threshold * threshold;
        for (const auto& pt : points) {
            double dx = pos.x() - pt.x();
            double dy = pos.y() - pt.y();
            if (dx * dx + dy * dy <= thresholdSq)
                return true;
        }
        return false;
    }
    default:
        return false;
    }
}

QList<HitTestResult> hitTest(const Tile& tile,
                             QPointF tileLocalPos,
                             double tileSize,
                             double hitRadiusPx,
                             const QSet<QString>& hiddenLayers,
                             const std::unordered_map<std::string, QColor>& layerColors)
{
    QList<HitTestResult> results;

    // Iterate in reverse draw order so topmost features come first
    for (int li = static_cast<int>(tile.layers.size()) - 1; li >= 0; --li) {
        const auto& layer = tile.layers[li];
        QString layerName = QString::fromStdString(layer.name);
        if (hiddenLayers.contains(layerName))
            continue;

        QColor color(100, 100, 100);
        auto it = layerColors.find(layer.name);
        if (it != layerColors.end())
            color = it->second;

        double extent = layer.extent;

        for (int fi = static_cast<int>(layer.features.size()) - 1; fi >= 0; --fi) {
            const auto& feature = layer.features[fi];
            if (feature.type == GeomType::Unknown)
                continue;

            if (hitTestFeature(feature, extent, tileSize, tileLocalPos, hitRadiusPx)) {
                HitTestResult result;
                result.layerName = layerName;
                result.layerColor = color;
                result.geomType = feature.type;
                result.featureId = feature.id;
                result.properties = decodeProperties(feature, layer);
                result.layerIndex = li;
                result.featureIndex = fi;
                results.append(result);
            }
        }
    }

    return results;
}

} // namespace mvt
