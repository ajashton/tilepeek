#include "map/WebMercator.h"

#include <QTest>
#include <cmath>

class TestWebMercator : public QObject {
    Q_OBJECT

private slots:
    void originAtZoom0()
    {
        // lon=0, lat=0 at zoom 0 should be center of the single 256x256 tile
        QCOMPARE(WebMercator::lonToPixelX(0.0, 0), 128.0);
        QCOMPARE(WebMercator::latToPixelY(0.0, 0), 128.0);
    }

    void westEdge()
    {
        // lon=-180 at any zoom should be pixel X = 0
        QCOMPARE(WebMercator::lonToPixelX(-180.0, 0), 0.0);
        QCOMPARE(WebMercator::lonToPixelX(-180.0, 5), 0.0);
    }

    void eastEdge()
    {
        // lon=180 at zoom 0 should be pixel X = 256
        QCOMPARE(WebMercator::lonToPixelX(180.0, 0), 256.0);
        QCOMPARE(WebMercator::lonToPixelX(180.0, 1), 512.0);
    }

    void mapSizeDoublesPerZoom()
    {
        QCOMPARE(WebMercator::mapSizePixels(0), 256);
        QCOMPARE(WebMercator::mapSizePixels(1), 512);
        QCOMPARE(WebMercator::mapSizePixels(4), 4096);
    }

    void tileIndexFromPixel()
    {
        QCOMPARE(WebMercator::pixelXToTileX(0.0), 0);
        QCOMPARE(WebMercator::pixelXToTileX(255.0), 0);
        QCOMPARE(WebMercator::pixelXToTileX(256.0), 1);
        QCOMPARE(WebMercator::pixelXToTileX(511.0), 1);
        QCOMPARE(WebMercator::pixelXToTileX(512.0), 2);
    }

    void lonRoundTrip()
    {
        for (double lon : {-180.0, -90.0, 0.0, 45.5, 180.0}) {
            double px = WebMercator::lonToPixelX(lon, 10);
            double result = WebMercator::pixelXToLon(px, 10);
            QVERIFY(std::abs(result - lon) < 1e-9);
        }
    }

    void latRoundTrip()
    {
        for (double lat : {-85.0, -45.0, 0.0, 37.7749, 85.0}) {
            double py = WebMercator::latToPixelY(lat, 10);
            double result = WebMercator::pixelYToLat(py, 10);
            QVERIFY(std::abs(result - lat) < 1e-6);
        }
    }

    void highZoomPrecision()
    {
        // At zoom 20, map is 256 * 2^20 = 268,435,456 pixels wide
        // San Francisco: lon=-122.4194, lat=37.7749
        double px = WebMercator::lonToPixelX(-122.4194, 20);
        double py = WebMercator::latToPixelY(37.7749, 20);

        // Verify round-trip at high zoom
        QVERIFY(std::abs(WebMercator::pixelXToLon(px, 20) - (-122.4194)) < 1e-6);
        QVERIFY(std::abs(WebMercator::pixelYToLat(py, 20) - 37.7749) < 1e-4);
    }

    void northernLatitudeAboveEquator()
    {
        // Northern latitudes should have pixel Y < 128 at zoom 0
        double py = WebMercator::latToPixelY(45.0, 0);
        QVERIFY(py < 128.0);
        QVERIFY(py > 0.0);
    }

    void southernLatitudeBelowEquator()
    {
        // Southern latitudes should have pixel Y > 128 at zoom 0
        double py = WebMercator::latToPixelY(-45.0, 0);
        QVERIFY(py > 128.0);
        QVERIFY(py < 256.0);
    }
};

QTEST_GUILESS_MAIN(TestWebMercator)
#include "tst_WebMercator.moc"
