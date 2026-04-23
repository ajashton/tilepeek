// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mvt/MvtTypes.h"

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class QPainter;

struct LabelCandidate {
    QString text;
    QPointF anchor;                      // tile-pixel space
    std::optional<double> angleRadians;  // present only for LineString
    QColor color;                        // layer color, full alpha
    mvt::GeomType geomType = mvt::GeomType::Unknown;
};

class LabelRenderer {
public:
    // Gather label candidates from visible layers that have labels enabled.
    // If clipToTile is true, drop candidates whose anchor is outside
    // [0, tileSize]^2 — this is the clipped render path used for the map,
    // where we rely on anchor-in-bounds as a cheap cross-tile dedup.
    static std::vector<LabelCandidate>
    collectCandidates(const mvt::Tile& tile,
                      const std::unordered_map<std::string, QColor>& layerColors,
                      const QSet<QString>& hiddenLayers,
                      const std::unordered_map<std::string, std::string>& labeledFields,
                      int tileSize,
                      bool clipToTile);

    // Lay out and draw candidates with simple axis-aligned collision.
    // Painter state (font, pen, brush, transform) is preserved.
    static void drawLabels(QPainter& painter,
                           const std::vector<LabelCandidate>& candidates,
                           int tileSize);

    // Exposed for tests.
    static QFont labelFont();
    static std::vector<std::size_t>
    selectNonColliding(const std::vector<QRectF>& boxes);
};
