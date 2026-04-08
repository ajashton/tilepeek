#include "util/TileCoords.h"

#include <QTest>

class TestTileCoords : public QObject {
    Q_OBJECT

private slots:
    void tmsToXyz_zoom0()
    {
        // At zoom 0 there's only one tile: TMS row 0 -> XYZ y 0
        QCOMPARE(TileCoords::tmsToXyz(0, 0), 0);
    }

    void tmsToXyz_zoom1()
    {
        // 2^1 = 2 tiles vertically. TMS row 0 (south) -> XYZ y 1 (bottom)
        QCOMPARE(TileCoords::tmsToXyz(1, 0), 1);
        QCOMPARE(TileCoords::tmsToXyz(1, 1), 0);
    }

    void tmsToXyz_specExample()
    {
        // From MBTiles spec: tile 11/327/791 is stored as TMS tile_row 1256
        // because 2^11 - 1 - 791 = 1256
        QCOMPARE(TileCoords::tmsToXyz(11, 1256), 791);
    }

    void xyzToTms_specExample()
    {
        // Inverse of the spec example
        QCOMPARE(TileCoords::xyzToTms(11, 791), 1256);
    }

    void roundtrip()
    {
        // The transform is its own inverse
        for (int zoom = 0; zoom <= 20; ++zoom) {
            int maxRow = (1 << zoom) - 1;
            for (int row : {0, maxRow / 2, maxRow}) {
                QCOMPARE(TileCoords::tmsToXyz(zoom, TileCoords::xyzToTms(zoom, row)), row);
                QCOMPARE(TileCoords::xyzToTms(zoom, TileCoords::tmsToXyz(zoom, row)), row);
            }
        }
    }

    void highZoom()
    {
        // zoom 20: 2^20 = 1048576 tiles
        QCOMPARE(TileCoords::tmsToXyz(20, 0), 1048575);
        QCOMPARE(TileCoords::tmsToXyz(20, 1048575), 0);
    }
};

QTEST_APPLESS_MAIN(TestTileCoords)
#include "tst_TileCoords.moc"
