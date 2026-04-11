#include "mvt/MvtGeometry.h"

#include <QTest>
#include <cmath>

using namespace mvt;

// Zigzag encode helper for building test data
static uint32_t zigzagEncode(int32_t n)
{
    return static_cast<uint32_t>((n << 1) ^ (n >> 31));
}

static uint32_t makeCommand(uint32_t id, uint32_t count)
{
    return (count << 3) | (id & 0x7);
}

class TestMvtGeometry : public QObject {
    Q_OBJECT

private slots:
    void decodeSinglePoint()
    {
        Feature f;
        f.type = GeomType::Point;
        // MoveTo(1) at (25, 17)
        f.geometry = {makeCommand(1, 1), zigzagEncode(25), zigzagEncode(17)};

        auto points = decodePoints(f, 4096.0, 256.0);
        QCOMPARE(points.size(), 1u);

        double expectedX = 25.0 / 4096.0 * 256.0;
        double expectedY = 17.0 / 4096.0 * 256.0;
        QVERIFY(std::abs(points[0].x() - expectedX) < 0.001);
        QVERIFY(std::abs(points[0].y() - expectedY) < 0.001);
    }

    void decodeMultiPoint()
    {
        Feature f;
        f.type = GeomType::Point;
        // MoveTo(2): (10,10) then delta (+5,+5) = (15,15)
        f.geometry = {makeCommand(1, 2),
                      zigzagEncode(10), zigzagEncode(10),
                      zigzagEncode(5), zigzagEncode(5)};

        auto points = decodePoints(f, 4096.0, 256.0);
        QCOMPARE(points.size(), 2u);
    }

    void decodeLineString()
    {
        Feature f;
        f.type = GeomType::LineString;
        // MoveTo(1) at (0,0), LineTo(2) to (100,0) then (100,100)
        f.geometry = {makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0),
                      makeCommand(2, 2), zigzagEncode(100), zigzagEncode(0),
                      zigzagEncode(0), zigzagEncode(100)};

        auto path = decodeGeometry(f, 4096.0, 256.0);
        QVERIFY(!path.isEmpty());
        // Path should have 3 points: moveTo + 2 lineTo
        QCOMPARE(path.elementCount(), 3);
    }

    void decodePolygon()
    {
        Feature f;
        f.type = GeomType::Polygon;
        // Triangle: MoveTo(0,0), LineTo(100,0), LineTo(100,100), ClosePath
        f.geometry = {makeCommand(1, 1), zigzagEncode(0), zigzagEncode(0),
                      makeCommand(2, 2), zigzagEncode(100), zigzagEncode(0),
                      zigzagEncode(0), zigzagEncode(100),
                      makeCommand(7, 1)};

        auto path = decodeGeometry(f, 4096.0, 256.0);
        QVERIFY(!path.isEmpty());
        // moveTo + 2 lineTo + close (which adds a lineTo back to start)
        QVERIFY(path.elementCount() >= 3);
    }

    void negativeDeltas()
    {
        Feature f;
        f.type = GeomType::Point;
        // MoveTo at (100, 100), then delta (-50, -30) = (50, 70)
        f.geometry = {makeCommand(1, 2),
                      zigzagEncode(100), zigzagEncode(100),
                      zigzagEncode(-50), zigzagEncode(-30)};

        auto points = decodePoints(f, 4096.0, 256.0);
        QCOMPARE(points.size(), 2u);

        double scale = 256.0 / 4096.0;
        QVERIFY(std::abs(points[0].x() - 100 * scale) < 0.001);
        QVERIFY(std::abs(points[0].y() - 100 * scale) < 0.001);
        QVERIFY(std::abs(points[1].x() - 50 * scale) < 0.001);
        QVERIFY(std::abs(points[1].y() - 70 * scale) < 0.001);
    }

    void coordinateScaling()
    {
        Feature f;
        f.type = GeomType::Point;
        // Point at extent corner (4096, 4096) should map to (256, 256)
        f.geometry = {makeCommand(1, 1), zigzagEncode(4096), zigzagEncode(4096)};

        auto points = decodePoints(f, 4096.0, 256.0);
        QCOMPARE(points.size(), 1u);
        QVERIFY(std::abs(points[0].x() - 256.0) < 0.001);
        QVERIFY(std::abs(points[0].y() - 256.0) < 0.001);
    }

    void emptyGeometry()
    {
        Feature f;
        f.type = GeomType::LineString;
        f.geometry = {};

        auto path = decodeGeometry(f, 4096.0, 256.0);
        QVERIFY(path.isEmpty());
    }

    void geometryBoundsWithinExtent()
    {
        Feature f;
        f.type = GeomType::LineString;
        // Line from (100, 200) to (3000, 3500)
        f.geometry = {makeCommand(1, 1), zigzagEncode(100), zigzagEncode(200),
                      makeCommand(2, 1), zigzagEncode(2900), zigzagEncode(3300)};

        auto bounds = geometryBounds(f);
        QVERIFY(!bounds.isNull());
        QCOMPARE(bounds.left(), 100.0);
        QCOMPARE(bounds.top(), 200.0);
        QCOMPARE(bounds.right(), 3000.0);
        QCOMPARE(bounds.bottom(), 3500.0);
    }

    void geometryBoundsWithBuffer()
    {
        Feature f;
        f.type = GeomType::LineString;
        // Line from (-256, -256) to (4352, 4352) — extends beyond [0, 4096]
        f.geometry = {makeCommand(1, 1), zigzagEncode(-256), zigzagEncode(-256),
                      makeCommand(2, 1), zigzagEncode(4608), zigzagEncode(4608)};

        auto bounds = geometryBounds(f);
        QVERIFY(!bounds.isNull());
        QCOMPARE(bounds.left(), -256.0);
        QCOMPARE(bounds.top(), -256.0);
        QCOMPARE(bounds.right(), 4352.0);
        QCOMPARE(bounds.bottom(), 4352.0);
    }

    void geometryBoundsEmpty()
    {
        Feature f;
        f.type = GeomType::Point;
        f.geometry = {};

        auto bounds = geometryBounds(f);
        QVERIFY(bounds.isNull());
    }
};

QTEST_GUILESS_MAIN(TestMvtGeometry)
#include "tst_MvtGeometry.moc"
