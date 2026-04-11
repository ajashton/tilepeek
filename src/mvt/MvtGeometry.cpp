#include "mvt/MvtGeometry.h"

#include <cstdint>

namespace mvt {

static int32_t zigzagDecode(uint32_t n)
{
    return static_cast<int32_t>((n >> 1) ^ (~(n & 1) + 1));
}

struct GeometryDecoder {
    const std::vector<uint32_t>& cmds;
    size_t pos = 0;
    int32_t cursorX = 0;
    int32_t cursorY = 0;
    double scale;

    GeometryDecoder(const std::vector<uint32_t>& cmds, double extent, double tileSize)
        : cmds(cmds)
        , scale(tileSize / extent)
    {
    }

    bool hasMore() const { return pos < cmds.size(); }

    void readCommand(uint32_t& id, uint32_t& count)
    {
        uint32_t cmd = cmds[pos++];
        id = cmd & 0x7;
        count = cmd >> 3;
    }

    QPointF readDelta()
    {
        int32_t dx = zigzagDecode(cmds[pos++]);
        int32_t dy = zigzagDecode(cmds[pos++]);
        cursorX += dx;
        cursorY += dy;
        return {cursorX * scale, cursorY * scale};
    }
};

QPainterPath decodeGeometry(const Feature& feature, double extent, double tileSize)
{
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);

    GeometryDecoder dec(feature.geometry, extent, tileSize);

    while (dec.hasMore()) {
        uint32_t id, count;
        dec.readCommand(id, count);

        switch (id) {
        case 1: // MoveTo
            for (uint32_t i = 0; i < count; ++i)
                path.moveTo(dec.readDelta());
            break;
        case 2: // LineTo
            for (uint32_t i = 0; i < count; ++i)
                path.lineTo(dec.readDelta());
            break;
        case 7: // ClosePath
            path.closeSubpath();
            break;
        default:
            break;
        }
    }

    return path;
}

std::vector<QPointF> decodePoints(const Feature& feature, double extent, double tileSize)
{
    std::vector<QPointF> points;
    GeometryDecoder dec(feature.geometry, extent, tileSize);

    while (dec.hasMore()) {
        uint32_t id, count;
        dec.readCommand(id, count);

        if (id == 1) { // MoveTo
            for (uint32_t i = 0; i < count; ++i)
                points.push_back(dec.readDelta());
        }
    }

    return points;
}

QRectF geometryBounds(const Feature& feature)
{
    if (feature.geometry.empty())
        return {};

    int32_t cursorX = 0, cursorY = 0;
    int32_t minX = INT32_MAX, minY = INT32_MAX;
    int32_t maxX = INT32_MIN, maxY = INT32_MIN;
    size_t pos = 0;
    const auto& cmds = feature.geometry;

    while (pos < cmds.size()) {
        uint32_t cmd = cmds[pos++];
        uint32_t id = cmd & 0x7;
        uint32_t count = cmd >> 3;

        if (id == 1 || id == 2) { // MoveTo or LineTo
            for (uint32_t i = 0; i < count && pos + 1 < cmds.size(); ++i) {
                cursorX += zigzagDecode(cmds[pos++]);
                cursorY += zigzagDecode(cmds[pos++]);
                minX = std::min(minX, cursorX);
                minY = std::min(minY, cursorY);
                maxX = std::max(maxX, cursorX);
                maxY = std::max(maxY, cursorY);
            }
        }
        // ClosePath (id==7) has no parameters
    }

    if (minX > maxX)
        return {};

    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

} // namespace mvt
