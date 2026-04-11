#include "mvt/FeatureHitTest.h"
#include "mvt/MvtTypes.h"

#include <QTest>

using namespace mvt;

static uint32_t zigzagEncode(int32_t n)
{
    return static_cast<uint32_t>((n << 1) ^ (n >> 31));
}

static uint32_t cmd(uint32_t id, uint32_t count)
{
    return (count << 3) | (id & 0x7);
}

class TestFeatureHitTest : public QObject {
    Q_OBJECT

private:
    // Build a tile with a single polygon (square from (0,0) to (2048,2048) in extent coords)
    Tile makeTileWithSquare()
    {
        Feature f;
        f.type = GeomType::Polygon;
        f.id = 42;
        f.tags = {0, 0}; // key[0] -> value[0]
        f.geometry = {cmd(1, 1), zigzagEncode(0), zigzagEncode(0),
                      cmd(2, 3), zigzagEncode(2048), zigzagEncode(0),
                      zigzagEncode(0), zigzagEncode(2048),
                      zigzagEncode(-2048), zigzagEncode(0),
                      cmd(7, 1)};

        Layer layer;
        layer.name = "test_layer";
        layer.extent = 4096;
        layer.features.push_back(f);
        layer.keys = {"name"};
        layer.values = {std::string("test_feature")};

        Tile tile;
        tile.layers.push_back(std::move(layer));
        return tile;
    }

    // Build a tile with a point at (1024, 1024) in extent coords
    Tile makeTileWithPoint()
    {
        Feature f;
        f.type = GeomType::Point;
        f.id = 7;
        f.tags = {0, 0};
        f.geometry = {cmd(1, 1), zigzagEncode(1024), zigzagEncode(1024)};

        Layer layer;
        layer.name = "points";
        layer.extent = 4096;
        layer.features.push_back(f);
        layer.keys = {"type"};
        layer.values = {std::string("marker")};

        Tile tile;
        tile.layers.push_back(std::move(layer));
        return tile;
    }

    // Build a tile with a line from (0, 2048) to (4096, 2048)
    Tile makeTileWithLine()
    {
        Feature f;
        f.type = GeomType::LineString;
        f.geometry = {cmd(1, 1), zigzagEncode(0), zigzagEncode(2048),
                      cmd(2, 1), zigzagEncode(4096), zigzagEncode(0)};

        Layer layer;
        layer.name = "lines";
        layer.extent = 4096;
        layer.features.push_back(f);

        Tile tile;
        tile.layers.push_back(std::move(layer));
        return tile;
    }

    std::unordered_map<std::string, QColor> defaultColors()
    {
        return {{"test_layer", QColor(255, 0, 0)},
                {"points", QColor(0, 255, 0)},
                {"lines", QColor(0, 0, 255)}};
    }

private slots:
    void hitInsidePolygon()
    {
        auto tile = makeTileWithSquare();
        // Point at center of the polygon (128, 128) in tile-local coords (tileSize=512)
        auto results = hitTest(tile, QPointF(128, 128), 512.0, 4.0,
                               {}, defaultColors());
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0].layerName, "test_layer");
        QCOMPARE(results[0].geomType, GeomType::Polygon);
        QCOMPARE(results[0].featureId.value(), uint64_t(42));
        QCOMPARE(results[0].properties.size(), 1);
        QCOMPARE(results[0].properties[0].first, "name");
        QCOMPARE(results[0].properties[0].second, "test_feature");
    }

    void missOutsidePolygon()
    {
        auto tile = makeTileWithSquare();
        // Point outside the polygon (400, 400) - the polygon covers [0, 256] in 512px tile
        auto results = hitTest(tile, QPointF(400, 400), 512.0, 4.0,
                               {}, defaultColors());
        QCOMPARE(results.size(), 0);
    }

    void hitPoint()
    {
        auto tile = makeTileWithPoint();
        // Point at (1024, 1024) extent -> (128, 128) in 512px tile
        auto results = hitTest(tile, QPointF(128, 128), 512.0, 4.0,
                               {}, defaultColors());
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0].layerName, "points");
        QCOMPARE(results[0].featureId.value(), uint64_t(7));
    }

    void missPointTooFar()
    {
        auto tile = makeTileWithPoint();
        // Point at (128, 128), clicking at (140, 128) = 12px away, beyond 4px radius
        auto results = hitTest(tile, QPointF(140, 128), 512.0, 4.0,
                               {}, defaultColors());
        QCOMPARE(results.size(), 0);
    }

    void hitLine()
    {
        auto tile = makeTileWithLine();
        // Line runs at y=256 in 512px tile (2048/4096*512). Click at (256, 256)
        auto results = hitTest(tile, QPointF(256, 256), 512.0, 4.0,
                               {}, defaultColors());
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0].layerName, "lines");
    }

    void missLineTooFar()
    {
        auto tile = makeTileWithLine();
        // Line at y=256, click at y=280 = 24px away
        auto results = hitTest(tile, QPointF(256, 280), 512.0, 4.0,
                               {}, defaultColors());
        QCOMPARE(results.size(), 0);
    }

    void hiddenLayerSkipped()
    {
        auto tile = makeTileWithSquare();
        QSet<QString> hidden = {"test_layer"};
        auto results = hitTest(tile, QPointF(128, 128), 512.0, 4.0,
                               hidden, defaultColors());
        QCOMPARE(results.size(), 0);
    }

    void valueToStringConversions()
    {
        QCOMPARE(valueToString(std::string("hello")), "hello");
        QCOMPARE(valueToString(true), "true");
        QCOMPARE(valueToString(false), "false");
        QCOMPARE(valueToString(int64_t(-42)), "-42");
        QCOMPARE(valueToString(uint64_t(100)), "100");
    }

    void multipleLayerResults()
    {
        // Build a tile with overlapping features from two layers
        Tile tile;

        Feature poly;
        poly.type = GeomType::Polygon;
        poly.id = 1;
        poly.geometry = {cmd(1, 1), zigzagEncode(0), zigzagEncode(0),
                         cmd(2, 3), zigzagEncode(4096), zigzagEncode(0),
                         zigzagEncode(0), zigzagEncode(4096),
                         zigzagEncode(-4096), zigzagEncode(0),
                         cmd(7, 1)};
        Layer layer1;
        layer1.name = "background";
        layer1.extent = 4096;
        layer1.features.push_back(poly);

        Feature point;
        point.type = GeomType::Point;
        point.id = 2;
        point.geometry = {cmd(1, 1), zigzagEncode(2048), zigzagEncode(2048)};
        Layer layer2;
        layer2.name = "foreground";
        layer2.extent = 4096;
        layer2.features.push_back(point);

        tile.layers.push_back(std::move(layer1));
        tile.layers.push_back(std::move(layer2));

        std::unordered_map<std::string, QColor> colors = {
            {"background", QColor(100, 100, 100)},
            {"foreground", QColor(255, 0, 0)}};

        // Click at center (256, 256) in 512px tile
        auto results = hitTest(tile, QPointF(256, 256), 512.0, 4.0, {}, colors);
        QCOMPARE(results.size(), 2);
        // Foreground (last drawn) should come first in results
        QCOMPARE(results[0].layerName, "foreground");
        QCOMPARE(results[1].layerName, "background");
    }
};

QTEST_GUILESS_MAIN(TestFeatureHitTest)
#include "tst_FeatureHitTest.moc"
