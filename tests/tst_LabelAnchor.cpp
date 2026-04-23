// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#include "mvt/LabelAnchor.h"

#include <QTest>
#include <cmath>
#include <numbers>

using namespace mvt;

static uint32_t zigzagEncode(int32_t n)
{
    return static_cast<uint32_t>((n << 1) ^ (n >> 31));
}

static uint32_t makeCommand(uint32_t id, uint32_t count)
{
    return (count << 3) | (id & 0x7);
}

class TestLabelAnchor : public QObject {
    Q_OBJECT

private slots:
    // -------- point --------

    void pointAnchorSingle()
    {
        Feature f;
        f.type = GeomType::Point;
        f.geometry = {makeCommand(1, 1), zigzagEncode(25), zigzagEncode(17)};

        auto anchor = pointLabelAnchor(f, 4096.0, 256.0);
        QVERIFY(anchor.has_value());
        QVERIFY(std::abs(anchor->x() - 25.0 / 4096.0 * 256.0) < 1e-6);
        QVERIFY(std::abs(anchor->y() - 17.0 / 4096.0 * 256.0) < 1e-6);
    }

    void pointAnchorMultiReturnsFirst()
    {
        Feature f;
        f.type = GeomType::Point;
        // Two absolute points: (10,10) then delta (+5,+5) -> (15,15)
        f.geometry = {makeCommand(1, 2),
                      zigzagEncode(10), zigzagEncode(10),
                      zigzagEncode(5), zigzagEncode(5)};

        auto anchor = pointLabelAnchor(f, 4096.0, 256.0);
        QVERIFY(anchor.has_value());
        QVERIFY(std::abs(anchor->x() - 10.0 / 4096.0 * 256.0) < 1e-6);
        QVERIFY(std::abs(anchor->y() - 10.0 / 4096.0 * 256.0) < 1e-6);
    }

    void pointAnchorEmpty()
    {
        Feature f;
        f.type = GeomType::Point;
        f.geometry = {};
        QVERIFY(!pointLabelAnchor(f, 4096.0, 256.0).has_value());
    }

    void pointAnchorWrongType()
    {
        Feature f;
        f.type = GeomType::LineString;
        f.geometry = {makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0)};
        QVERIFY(!pointLabelAnchor(f, 4096.0, 256.0).has_value());
    }

    // -------- line --------

    void lineAnchorPicksLongestSegment()
    {
        // Extent=4096, tileSize=256 -> scale=1/16
        // MoveTo(0,0), LineTo(10,0) [len=10], LineTo(10,100) [len=100].
        Feature f;
        f.type = GeomType::LineString;
        f.geometry = {makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0),
                      makeCommand(2, 2),
                      zigzagEncode(10), zigzagEncode(0),
                      zigzagEncode(0), zigzagEncode(100)};

        auto anchor = lineLabelAnchor(f, 4096.0, 256.0);
        QVERIFY(anchor.has_value());
        // Longest segment is from (10,0) to (10,100) in tile coords; in pixel
        // space (scale=1/16) midpoint is (10/16, 50/16) = (0.625, 3.125).
        QVERIFY(std::abs(anchor->point.x() - 0.625) < 1e-6);
        QVERIFY(std::abs(anchor->point.y() - 3.125) < 1e-6);
        // Segment direction pi/2 would be upside-down-text-bad; normalizer
        // keeps angle within (-pi/2, pi/2]. pi/2 stays as pi/2 (boundary).
        QVERIFY(std::abs(std::abs(anchor->angleRadians) - std::numbers::pi / 2.0) < 1e-6);
    }

    void lineAnchorMultiLineStringPicksGlobalLongest()
    {
        // Two subpaths; second contains the longest segment.
        // Subpath 1: (0,0) -> (10,0) [len=10 tile units]
        // Subpath 2: (100,100) -> (100,200) [len=100 tile units]
        Feature f;
        f.type = GeomType::LineString;
        f.geometry = {makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0),
                      makeCommand(2, 1), zigzagEncode(10), zigzagEncode(0),
                      makeCommand(1, 1),
                      // Delta from (10,0) to (100,100): +90, +100
                      zigzagEncode(90), zigzagEncode(100),
                      makeCommand(2, 1),
                      zigzagEncode(0), zigzagEncode(100)};

        auto anchor = lineLabelAnchor(f, 4096.0, 256.0);
        QVERIFY(anchor.has_value());
        // Midpoint of (100,100)->(100,200) in tile units -> (100, 150).
        // In pixel units (scale=1/16): (6.25, 9.375).
        QVERIFY(std::abs(anchor->point.x() - 6.25) < 1e-6);
        QVERIFY(std::abs(anchor->point.y() - 9.375) < 1e-6);
    }

    void lineAnchorNoLineTo()
    {
        // Only a MoveTo, no segments.
        Feature f;
        f.type = GeomType::LineString;
        f.geometry = {makeCommand(1, 1), zigzagEncode(5), zigzagEncode(5)};
        QVERIFY(!lineLabelAnchor(f, 4096.0, 256.0).has_value());
    }

    void lineAnchorAngleStaysReadable()
    {
        // Segment from (100,0) -> (0,0): direction is pi (left). We expect
        // the normalizer to collapse this to 0 (readable, text not reversed).
        Feature f;
        f.type = GeomType::LineString;
        f.geometry = {makeCommand(1, 1), zigzagEncode(100), zigzagEncode(0),
                      makeCommand(2, 1),
                      zigzagEncode(-100), zigzagEncode(0)};
        auto anchor = lineLabelAnchor(f, 4096.0, 256.0);
        QVERIFY(anchor.has_value());
        QVERIFY(std::abs(anchor->angleRadians) < 1e-6);
    }

    // -------- polygon --------

    void polygonAnchorSquareCentroidAtCenter()
    {
        // Square with corners at (0,0), (100,0), (100,100), (0,100) in tile
        // units. Winding order here is CW in Y-down coords -> negative area.
        Feature f;
        f.type = GeomType::Polygon;
        f.geometry = {makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0),
                      makeCommand(2, 3),
                      zigzagEncode(100), zigzagEncode(0),
                      zigzagEncode(0), zigzagEncode(100),
                      zigzagEncode(-100), zigzagEncode(0),
                      makeCommand(7, 1)};

        auto anchor = polygonLabelAnchor(f, 4096.0, 256.0);
        QVERIFY(anchor.has_value());
        // Centroid (50,50) in tile units -> pixel (50/16, 50/16) = 3.125
        QVERIFY(std::abs(anchor->x() - 3.125) < 1e-6);
        QVERIFY(std::abs(anchor->y() - 3.125) < 1e-6);
    }

    void polygonAnchorIgnoresInnerRing()
    {
        // Outer CW square 0..100 with inner CCW square 40..60. Centroid
        // should be the outer ring's centroid.
        Feature f;
        f.type = GeomType::Polygon;
        f.geometry = {
            // Outer CW: (0,0) (100,0) (100,100) (0,100)
            makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0),
            makeCommand(2, 3),
            zigzagEncode(100), zigzagEncode(0),
            zigzagEncode(0), zigzagEncode(100),
            zigzagEncode(-100), zigzagEncode(0),
            makeCommand(7, 1),
            // Inner CCW starting at (40,40): (40,40)->(40,60)->(60,60)->(60,40)
            // Delta from (0,100) to (40,40) is (40,-60).
            makeCommand(1, 1), zigzagEncode(40), zigzagEncode(-60),
            makeCommand(2, 3),
            zigzagEncode(0), zigzagEncode(20),
            zigzagEncode(20), zigzagEncode(0),
            zigzagEncode(0), zigzagEncode(-20),
            makeCommand(7, 1)};

        auto anchor = polygonLabelAnchor(f, 4096.0, 256.0);
        QVERIFY(anchor.has_value());
        QVERIFY(std::abs(anchor->x() - 3.125) < 1e-6);
        QVERIFY(std::abs(anchor->y() - 3.125) < 1e-6);
    }

    void polygonAnchorMultiPolygonPicksLargestOuter()
    {
        // Two CW outer rings: small 0..10, large 200..400.
        // Small centroid = (5,5); large centroid = (300,300).
        Feature f;
        f.type = GeomType::Polygon;
        f.geometry = {
            // Small CW square
            makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0),
            makeCommand(2, 3),
            zigzagEncode(10), zigzagEncode(0),
            zigzagEncode(0), zigzagEncode(10),
            zigzagEncode(-10), zigzagEncode(0),
            makeCommand(7, 1),
            // Large CW square; delta from (0,10) to (200,200) = (200,190)
            makeCommand(1, 1), zigzagEncode(200), zigzagEncode(190),
            makeCommand(2, 3),
            zigzagEncode(200), zigzagEncode(0),
            zigzagEncode(0), zigzagEncode(200),
            zigzagEncode(-200), zigzagEncode(0),
            makeCommand(7, 1)};

        auto anchor = polygonLabelAnchor(f, 4096.0, 256.0);
        QVERIFY(anchor.has_value());
        // Expect centroid (300,300) in tile units -> 300/16 = 18.75 px.
        QVERIFY(std::abs(anchor->x() - 18.75) < 1e-6);
        QVERIFY(std::abs(anchor->y() - 18.75) < 1e-6);
    }

    void polygonAnchorZeroArea()
    {
        // Colinear "ring" of three points on the x axis.
        Feature f;
        f.type = GeomType::Polygon;
        f.geometry = {makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0),
                      makeCommand(2, 2),
                      zigzagEncode(10), zigzagEncode(0),
                      zigzagEncode(10), zigzagEncode(0),
                      makeCommand(7, 1)};
        QVERIFY(!polygonLabelAnchor(f, 4096.0, 256.0).has_value());
    }

    void polygonAnchorEmpty()
    {
        Feature f;
        f.type = GeomType::Polygon;
        QVERIFY(!polygonLabelAnchor(f, 4096.0, 256.0).has_value());
    }
};

QTEST_GUILESS_MAIN(TestLabelAnchor)
#include "tst_LabelAnchor.moc"
