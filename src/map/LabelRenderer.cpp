// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#include "map/LabelRenderer.h"

#include "mvt/FeatureHitTest.h"
#include "mvt/LabelAnchor.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QString>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPointAnchorGapPx = 2.0;   // gap between geometry and text
constexpr double kBoxPaddingPx = 2.0;        // collision-box padding
constexpr double kHaloPenWidthPx = 3.0;

// Compute the axis-aligned bbox of `unrotated` after rotating it in place
// around its center by `angleRadians`.
QRectF rotatedAabb(const QRectF& unrotated, double angleRadians)
{
    const double c = std::cos(angleRadians);
    const double s = std::sin(angleRadians);
    const double hw = unrotated.width() * 0.5;
    const double hh = unrotated.height() * 0.5;
    const double halfExtentX = std::abs(hw * c) + std::abs(hh * s);
    const double halfExtentY = std::abs(hw * s) + std::abs(hh * c);
    const QPointF center = unrotated.center();
    return QRectF(center.x() - halfExtentX, center.y() - halfExtentY,
                  2.0 * halfExtentX, 2.0 * halfExtentY);
}

} // namespace

QFont LabelRenderer::labelFont()
{
    QFont f;
    const double pt = f.pointSizeF();
    if (pt > 0.0)
        f.setPointSizeF(std::max(1.0, pt - 1.0));
    else
        f.setPixelSize(std::max(1, f.pixelSize() - 1));
    f.setStretch(QFont::Condensed);
    return f;
}

std::vector<std::size_t>
LabelRenderer::selectNonColliding(const std::vector<QRectF>& boxes)
{
    std::vector<std::size_t> accepted;
    accepted.reserve(boxes.size());
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        bool collides = false;
        for (std::size_t j : accepted) {
            if (boxes[i].intersects(boxes[j])) {
                collides = true;
                break;
            }
        }
        if (!collides)
            accepted.push_back(i);
    }
    return accepted;
}

std::vector<LabelCandidate>
LabelRenderer::collectCandidates(const mvt::Tile& tile,
                                 const std::unordered_map<std::string, QColor>& layerColors,
                                 const QSet<QString>& hiddenLayers,
                                 const std::unordered_map<std::string, std::string>& labeledFields,
                                 int tileSize,
                                 bool clipToTile)
{
    std::vector<LabelCandidate> out;
    if (labeledFields.empty())
        return out;

    const double tileSizeD = static_cast<double>(tileSize);

    auto inBounds = [&](const QPointF& p) {
        if (!clipToTile)
            return true;
        return p.x() >= 0.0 && p.x() <= tileSizeD
            && p.y() >= 0.0 && p.y() <= tileSizeD;
    };

    for (const auto& layer : tile.layers) {
        auto fieldIt = labeledFields.find(layer.name);
        if (fieldIt == labeledFields.end())
            continue;
        QString layerName = QString::fromStdString(layer.name);
        if (hiddenLayers.contains(layerName))
            continue;

        QColor color(100, 100, 100);
        auto colorIt = layerColors.find(layer.name);
        if (colorIt != layerColors.end())
            color = colorIt->second;
        color.setAlpha(255); // labels render at full alpha

        const double extent = static_cast<double>(layer.extent);
        const std::string& key = fieldIt->second;

        for (const auto& feature : layer.features) {
            auto text = mvt::featurePropertyAsString(feature, layer, key);
            if (!text || text->isEmpty())
                continue;

            switch (feature.type) {
            case mvt::GeomType::Point: {
                auto anchor = mvt::pointLabelAnchor(feature, extent, tileSizeD);
                if (!anchor || !inBounds(*anchor))
                    break;
                out.push_back({*text, *anchor, std::nullopt, color, feature.type});
                break;
            }
            case mvt::GeomType::LineString: {
                auto anchor = mvt::lineLabelAnchor(feature, extent, tileSizeD);
                if (!anchor || !inBounds(anchor->point))
                    break;
                out.push_back({*text, anchor->point, anchor->angleRadians, color,
                               feature.type});
                break;
            }
            case mvt::GeomType::Polygon: {
                auto anchor = mvt::polygonLabelAnchor(feature, extent, tileSizeD);
                if (!anchor || !inBounds(*anchor))
                    break;
                out.push_back({*text, *anchor, std::nullopt, color, feature.type});
                break;
            }
            default:
                break;
            }
        }
    }

    return out;
}

void LabelRenderer::drawLabels(QPainter& painter,
                               const std::vector<LabelCandidate>& candidates,
                               int /*tileSize*/)
{
    if (candidates.empty())
        return;

    painter.save();

    const QFont font = labelFont();
    painter.setFont(font);
    const QFontMetricsF fm(font);

    // Precompute an unrotated bbox (centered on the anchor) and the collision
    // AABB (rotation-expanded for line labels) for each candidate.
    struct Layout {
        QRectF unrotatedBbox;  // bbox centered on the logical label origin
        QRectF collisionAabb;  // axis-aligned envelope for collision
        QPointF labelCenter;   // center of the unrotated bbox in tile space
    };
    std::vector<Layout> layouts;
    layouts.reserve(candidates.size());

    std::vector<QRectF> collisionBoxes;
    collisionBoxes.reserve(candidates.size());

    for (const auto& c : candidates) {
        QRectF tight = fm.tightBoundingRect(c.text);
        double w = tight.width() + 2.0 * kBoxPaddingPx;
        double h = tight.height() + 2.0 * kBoxPaddingPx;

        QPointF center = c.anchor;
        if (c.geomType == mvt::GeomType::Point) {
            // Place text above the anchor with a small gap.
            center.ry() -= (h * 0.5 + kPointAnchorGapPx);
        }
        // Polygon + line default: centered on anchor.

        QRectF unrotated(center.x() - w * 0.5, center.y() - h * 0.5, w, h);
        QRectF aabb = c.angleRadians ? rotatedAabb(unrotated, *c.angleRadians)
                                      : unrotated;

        layouts.push_back({unrotated, aabb, center});
        collisionBoxes.push_back(aabb);
    }

    const auto accepted = selectNonColliding(collisionBoxes);

    for (std::size_t idx : accepted) {
        const auto& c = candidates[idx];
        const auto& lay = layouts[idx];
        QRectF tight = fm.tightBoundingRect(c.text);

        // Text baseline origin such that the glyph bounding rect is centered
        // on lay.labelCenter. QPainterPath::addText places glyphs with their
        // baseline at the given point; tight.y() is typically negative and
        // equals -ascent for the specific text.
        QPointF baselineOrigin(
            lay.labelCenter.x() - tight.width() * 0.5 - tight.x(),
            lay.labelCenter.y() + tight.height() * 0.5 - (tight.y() + tight.height()));

        QPainterPath path;
        path.addText(baselineOrigin, font, c.text);

        painter.save();
        if (c.angleRadians) {
            painter.translate(lay.labelCenter);
            painter.rotate(*c.angleRadians * 180.0 / M_PI);
            painter.translate(-lay.labelCenter);
        }

        QPen halo(QColor(0, 0, 0, 160));
        halo.setWidthF(kHaloPenWidthPx);
        halo.setCapStyle(Qt::RoundCap);
        halo.setJoinStyle(Qt::RoundJoin);
        painter.strokePath(path, halo);
        painter.fillPath(path, c.color);

        painter.restore();
    }

    painter.restore();
}
