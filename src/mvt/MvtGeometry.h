#pragma once

#include "mvt/MvtTypes.h"

#include <QPainterPath>
#include <QPointF>
#include <vector>

namespace mvt {

QPainterPath decodeGeometry(const Feature& feature, double extent, double tileSize);
std::vector<QPointF> decodePoints(const Feature& feature, double extent, double tileSize);

} // namespace mvt
