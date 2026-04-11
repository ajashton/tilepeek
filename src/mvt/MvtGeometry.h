#pragma once

#include "mvt/MvtTypes.h"

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <vector>

namespace mvt {

QPainterPath decodeGeometry(const Feature& feature, double extent, double tileSize);
std::vector<QPointF> decodePoints(const Feature& feature, double extent, double tileSize);

// Scan geometry commands to find the bounding box in tile coordinate space.
// Coordinates are relative to [0, extent]; values outside that range indicate buffer data.
QRectF geometryBounds(const Feature& feature);

} // namespace mvt
