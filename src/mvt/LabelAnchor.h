// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mvt/MvtTypes.h"

#include <QPointF>
#include <optional>

namespace mvt {

struct LineAnchor {
    QPointF point;        // midpoint of longest segment, tile-pixel space
    double angleRadians;  // segment direction, normalized to (-pi/2, pi/2]
    double lengthPx;      // length of chosen segment in tile-pixel space
};

// Anchor point for a point/multipoint feature: the first decoded vertex.
std::optional<QPointF> pointLabelAnchor(const Feature& feature,
                                        double extent, double tileSize);

// Anchor for a (multi)linestring: midpoint, direction, and length of the
// longest straight segment across all subpaths.
std::optional<LineAnchor> lineLabelAnchor(const Feature& feature,
                                          double extent, double tileSize);

// Anchor for a (multi)polygon: area-weighted centroid of the largest outer
// ring. "Outer" is detected by signed area sign per MVT v2; falls back to
// the largest |area| ring when no negative-area ring is present.
std::optional<QPointF> polygonLabelAnchor(const Feature& feature,
                                          double extent, double tileSize);

} // namespace mvt
