// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#include "mvt/LabelAnchor.h"
#include "mvt/MvtGeometry.h"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace mvt {

namespace {

int32_t zigzagDecode(uint32_t n)
{
    return static_cast<int32_t>((n >> 1) ^ (~(n & 1) + 1));
}

// Normalize an angle to (-pi/2, pi/2] so labels never render upside-down.
double normalizeReadableAngle(double angle)
{
    constexpr double kPi = std::numbers::pi;
    while (angle > kPi / 2.0)
        angle -= kPi;
    while (angle <= -kPi / 2.0)
        angle += kPi;
    return angle;
}

} // namespace

std::optional<QPointF> pointLabelAnchor(const Feature& feature,
                                        double extent, double tileSize)
{
    if (feature.type != GeomType::Point)
        return std::nullopt;
    auto points = decodePoints(feature, extent, tileSize);
    if (points.empty())
        return std::nullopt;
    return points.front();
}

std::optional<LineAnchor> lineLabelAnchor(const Feature& feature,
                                          double extent, double tileSize)
{
    if (feature.type != GeomType::LineString || feature.geometry.empty())
        return std::nullopt;

    const double scale = tileSize / extent;
    int32_t cursorX = 0, cursorY = 0;
    bool haveCursor = false;

    double bestLenSq = 0.0;
    QPointF bestMid{};
    double bestAngle = 0.0;

    const auto& cmds = feature.geometry;
    size_t pos = 0;
    while (pos < cmds.size()) {
        uint32_t cmd = cmds[pos++];
        uint32_t id = cmd & 0x7;
        uint32_t count = cmd >> 3;

        if (id == 1) { // MoveTo
            for (uint32_t i = 0; i < count && pos + 1 < cmds.size(); ++i) {
                cursorX += zigzagDecode(cmds[pos++]);
                cursorY += zigzagDecode(cmds[pos++]);
                haveCursor = true;
            }
        } else if (id == 2) { // LineTo
            for (uint32_t i = 0; i < count && pos + 1 < cmds.size(); ++i) {
                int32_t prevX = cursorX;
                int32_t prevY = cursorY;
                cursorX += zigzagDecode(cmds[pos++]);
                cursorY += zigzagDecode(cmds[pos++]);
                if (!haveCursor)
                    continue;

                double ax = prevX * scale;
                double ay = prevY * scale;
                double bx = cursorX * scale;
                double by = cursorY * scale;
                double dx = bx - ax;
                double dy = by - ay;
                double lenSq = dx * dx + dy * dy;
                if (lenSq > bestLenSq) {
                    bestLenSq = lenSq;
                    bestMid = QPointF((ax + bx) * 0.5, (ay + by) * 0.5);
                    bestAngle = normalizeReadableAngle(std::atan2(dy, dx));
                }
            }
        } else {
            // ClosePath (id == 7) has no params. Other ids shouldn't appear
            // in linestrings but we skip defensively.
        }
    }

    if (bestLenSq <= 0.0)
        return std::nullopt;

    return LineAnchor{bestMid, bestAngle, std::sqrt(bestLenSq)};
}

std::optional<QPointF> polygonLabelAnchor(const Feature& feature,
                                          double extent, double tileSize)
{
    if (feature.type != GeomType::Polygon || feature.geometry.empty())
        return std::nullopt;

    const double scale = tileSize / extent;

    // Collect each ring's vertices in tile-pixel space, then compute its
    // signed area via shoelace after all rings are gathered. A ring is
    // delimited by MoveTo (start) and optionally ClosePath.
    std::vector<QPointF> current;
    struct Ring {
        std::vector<QPointF> pts;
        double signedArea;
    };
    std::vector<Ring> rings;

    auto finishRing = [&]() {
        if (current.size() < 3) {
            current.clear();
            return;
        }
        double a = 0.0;
        for (size_t i = 0, n = current.size(); i < n; ++i) {
            const auto& p = current[i];
            const auto& q = current[(i + 1) % n];
            a += p.x() * q.y() - q.x() * p.y();
        }
        a *= 0.5;
        rings.push_back({std::move(current), a});
        current.clear();
    };

    int32_t cursorX = 0, cursorY = 0;
    const auto& cmds = feature.geometry;
    size_t pos = 0;

    while (pos < cmds.size()) {
        uint32_t cmd = cmds[pos++];
        uint32_t id = cmd & 0x7;
        uint32_t count = cmd >> 3;

        if (id == 1) { // MoveTo: starts a new ring
            finishRing();
            for (uint32_t i = 0; i < count && pos + 1 < cmds.size(); ++i) {
                cursorX += zigzagDecode(cmds[pos++]);
                cursorY += zigzagDecode(cmds[pos++]);
                current.emplace_back(cursorX * scale, cursorY * scale);
            }
        } else if (id == 2) { // LineTo
            for (uint32_t i = 0; i < count && pos + 1 < cmds.size(); ++i) {
                cursorX += zigzagDecode(cmds[pos++]);
                cursorY += zigzagDecode(cmds[pos++]);
                current.emplace_back(cursorX * scale, cursorY * scale);
            }
        } else if (id == 7) { // ClosePath
            finishRing();
        }
    }
    finishRing();

    if (rings.empty())
        return std::nullopt;

    // In tile-pixel (Y-down) space, MVT v2 exterior rings (CW) yield negative
    // signed area under the standard shoelace formula. Prefer the outer ring
    // with the largest |area|. If none is negative (MVT v1 has ambiguous
    // winding), fall back to the overall largest |area| ring.
    const Ring* best = nullptr;
    double bestArea = 0.0;
    for (const auto& r : rings) {
        if (r.signedArea < 0.0) {
            double absA = -r.signedArea;
            if (absA > bestArea) {
                bestArea = absA;
                best = &r;
            }
        }
    }
    if (!best) {
        for (const auto& r : rings) {
            double absA = std::abs(r.signedArea);
            if (absA > bestArea) {
                bestArea = absA;
                best = &r;
            }
        }
    }
    if (!best || bestArea <= 0.0)
        return std::nullopt;

    // Area-weighted centroid of the chosen ring.
    double cx = 0.0, cy = 0.0;
    const auto& pts = best->pts;
    for (size_t i = 0, n = pts.size(); i < n; ++i) {
        const auto& p = pts[i];
        const auto& q = pts[(i + 1) % n];
        double cross = p.x() * q.y() - q.x() * p.y();
        cx += (p.x() + q.x()) * cross;
        cy += (p.y() + q.y()) * cross;
    }
    double factor = 6.0 * best->signedArea;
    if (factor == 0.0)
        return std::nullopt;
    return QPointF(cx / factor, cy / factor);
}

} // namespace mvt
