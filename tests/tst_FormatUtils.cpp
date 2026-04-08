#include "util/FormatUtils.h"

#include <QTest>

class TestFormatUtils : public QObject {
    Q_OBJECT

private slots:
    void zeroBytes()
    {
        QCOMPARE(FormatUtils::formatTileSize(0), "0 B");
    }

    void smallBytes()
    {
        QCOMPARE(FormatUtils::formatTileSize(1), "1 B");
        QCOMPARE(FormatUtils::formatTileSize(512), "512 B");
        QCOMPARE(FormatUtils::formatTileSize(1023), "1023 B");
    }

    void exactlyOneKiB()
    {
        QCOMPARE(FormatUtils::formatTileSize(1024), "1.0 KiB");
    }

    void fractionalKiB()
    {
        // 1536 bytes = 1.5 KiB
        QCOMPARE(FormatUtils::formatTileSize(1536), "1.5 KiB");
    }

    void largeValueStaysKiB()
    {
        // 1,536,000 bytes = 1500.0 KiB (not converted to MiB)
        QCOMPARE(FormatUtils::formatTileSize(1536000), "1500.0 KiB");
    }

    void roundingToOneDecimal()
    {
        // 2560 bytes = 2.5 KiB exactly
        QCOMPARE(FormatUtils::formatTileSize(2560), "2.5 KiB");
        // 2600 bytes = 2.5390625 KiB -> "2.5 KiB"
        QCOMPARE(FormatUtils::formatTileSize(2600), "2.5 KiB");
        // 2700 bytes = 2.63671875 KiB -> "2.6 KiB"
        QCOMPARE(FormatUtils::formatTileSize(2700), "2.6 KiB");
    }
};

QTEST_GUILESS_MAIN(TestFormatUtils)
#include "tst_FormatUtils.moc"
