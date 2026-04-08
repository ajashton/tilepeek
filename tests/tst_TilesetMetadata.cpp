#include "model/TilesetMetadata.h"

#include <QTest>

class TestTilesetMetadata : public QObject {
    Q_OBJECT

private slots:
    void addAndRetrieve()
    {
        TilesetMetadata md;
        md.addField("name", "Test Tileset", FieldCategory::Required);
        md.addField("bounds", "-180,-85,180,85", FieldCategory::Recommended);
        md.addField("custom", "value", FieldCategory::NonStandard);

        QCOMPARE(md.fields().size(), 3);
    }

    void fieldsByCategory()
    {
        TilesetMetadata md;
        md.addField("name", "Test", FieldCategory::Required);
        md.addField("format", "png", FieldCategory::Required);
        md.addField("bounds", "-180,-85,180,85", FieldCategory::Recommended);

        auto required = md.fieldsByCategory(FieldCategory::Required);
        QCOMPARE(required.size(), 2);
        QCOMPARE(required[0].name, "name");
        QCOMPARE(required[1].name, "format");

        auto recommended = md.fieldsByCategory(FieldCategory::Recommended);
        QCOMPARE(recommended.size(), 1);

        auto standard = md.fieldsByCategory(FieldCategory::Standard);
        QCOMPARE(standard.size(), 0);
    }

    void valueLookup()
    {
        TilesetMetadata md;
        md.addField("name", "Test", FieldCategory::Required);

        auto val = md.value("name");
        QVERIFY(val.has_value());
        QCOMPARE(*val, "Test");

        QVERIFY(!md.value("missing").has_value());
    }

    void hasField()
    {
        TilesetMetadata md;
        md.addField("format", "png", FieldCategory::Required);

        QVERIFY(md.hasField("format"));
        QVERIFY(!md.hasField("name"));
    }

    void clear()
    {
        TilesetMetadata md;
        md.addField("name", "Test", FieldCategory::Required);
        QCOMPARE(md.fields().size(), 1);

        md.clear();
        QCOMPARE(md.fields().size(), 0);
        QVERIFY(!md.hasField("name"));
    }
};

QTEST_APPLESS_MAIN(TestTilesetMetadata)
#include "tst_TilesetMetadata.moc"
