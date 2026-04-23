// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mvt/MvtTypes.h"

#include <QColor>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QSet>
#include <QString>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mvt {

struct HitTestResult {
    QString layerName;
    QColor layerColor;
    GeomType geomType;
    std::optional<uint64_t> featureId;
    QVector<std::pair<QString, QString>> properties;
    int layerIndex;
    int featureIndex;
};

struct FeatureHighlight {
    QPainterPath path;
    std::vector<QPointF> points;
    QColor color;
    GeomType type;
    QString layerName;
};

QList<HitTestResult> hitTest(const Tile& tile,
                             QPointF tileLocalPos,
                             double tileSize,
                             double hitRadiusPx,
                             double pointRadiusPx,
                             const QSet<QString>& hiddenLayers,
                             const std::unordered_map<std::string, QColor>& layerColors);

QString valueToString(const Value& value);

// Look up a feature property by key name. Returns std::nullopt if the feature
// does not carry that key in its tags. O(n) over the feature's tags list.
std::optional<QString> featurePropertyAsString(const Feature& feature,
                                               const Layer& layer,
                                               const std::string& key);

} // namespace mvt
