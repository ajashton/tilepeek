#include "mbtiles/MBTilesMetadataParser.h"

#include <QTest>

class TestMBTilesMetadataParser : public QObject {
    Q_OBJECT

private slots:
    void parseBounds_valid()
    {
        auto b = MBTilesMetadataParser::parseBounds("-180.0,-85,180,85");
        QVERIFY(b.has_value());
        QCOMPARE(b->left, -180.0);
        QCOMPARE(b->bottom, -85.0);
        QCOMPARE(b->right, 180.0);
        QCOMPARE(b->top, 85.0);
    }

    void parseBounds_withSpaces()
    {
        auto b = MBTilesMetadataParser::parseBounds(" -10.5 , 20.3 , 30.1 , 40.2 ");
        QVERIFY(b.has_value());
        QCOMPARE(b->left, -10.5);
    }

    void parseBounds_wrongCount()
    {
        QVERIFY(!MBTilesMetadataParser::parseBounds("1,2,3").has_value());
        QVERIFY(!MBTilesMetadataParser::parseBounds("1,2,3,4,5").has_value());
    }

    void parseBounds_nonNumeric()
    {
        QVERIFY(!MBTilesMetadataParser::parseBounds("a,b,c,d").has_value());
    }

    void parseBounds_empty()
    {
        QVERIFY(!MBTilesMetadataParser::parseBounds("").has_value());
    }

    void parseCenter_valid()
    {
        auto c = MBTilesMetadataParser::parseCenter("-122.1906,37.7599,11");
        QVERIFY(c.has_value());
        QCOMPARE(c->longitude, -122.1906);
        QCOMPARE(c->latitude, 37.7599);
        QCOMPARE(c->zoom, 11.0);
    }

    void parseCenter_wrongCount()
    {
        QVERIFY(!MBTilesMetadataParser::parseCenter("1,2").has_value());
        QVERIFY(!MBTilesMetadataParser::parseCenter("1,2,3,4").has_value());
    }

    void parseCenter_nonNumeric()
    {
        QVERIFY(!MBTilesMetadataParser::parseCenter("x,y,z").has_value());
    }

    void longitudeRange()
    {
        QVERIFY(MBTilesMetadataParser::isLongitudeInRange(0.0));
        QVERIFY(MBTilesMetadataParser::isLongitudeInRange(-180.0));
        QVERIFY(MBTilesMetadataParser::isLongitudeInRange(180.0));
        QVERIFY(!MBTilesMetadataParser::isLongitudeInRange(-180.1));
        QVERIFY(!MBTilesMetadataParser::isLongitudeInRange(180.1));
    }

    void latitudeRange()
    {
        QVERIFY(MBTilesMetadataParser::isLatitudeInRange(0.0));
        QVERIFY(MBTilesMetadataParser::isLatitudeInRange(-85.051129));
        QVERIFY(MBTilesMetadataParser::isLatitudeInRange(85.051129));
        QVERIFY(!MBTilesMetadataParser::isLatitudeInRange(-85.06));
        QVERIFY(!MBTilesMetadataParser::isLatitudeInRange(85.06));
    }

    void parse_allFieldsPresent()
    {
        QList<std::pair<QString, QString>> raw = {
            {"name", "Test"},
            {"format", "png"},
            {"bounds", "-180,-85,180,85"},
            {"center", "0,0,5"},
            {"minzoom", "0"},
            {"maxzoom", "10"},
            {"description", "A test tileset"},
        };

        auto result = MBTilesMetadataParser::parse(raw, ZoomRange{0, 10});

        // No errors expected
        bool hasError = false;
        for (const auto& msg : result.messages) {
            if (msg.level == ValidationMessage::Level::Error)
                hasError = true;
        }
        QVERIFY(!hasError);

        QVERIFY(result.metadata.hasField("name"));
        QCOMPARE(result.metadata.value("format").value(), "png");

        // Check categorization
        auto required = result.metadata.fieldsByCategory(FieldCategory::Required);
        QCOMPARE(required.size(), 2);

        auto standard = result.metadata.fieldsByCategory(FieldCategory::Standard);
        QCOMPARE(standard.size(), 1);
        QCOMPARE(standard[0].name, "description");
    }

    void parse_missingRequired()
    {
        QList<std::pair<QString, QString>> raw = {
            {"bounds", "-180,-85,180,85"},
        };

        auto result = MBTilesMetadataParser::parse(raw, std::nullopt);

        // Should have errors for missing name and format
        int errorCount = 0;
        for (const auto& msg : result.messages) {
            if (msg.level == ValidationMessage::Level::Error)
                errorCount++;
        }
        QVERIFY(errorCount >= 2);
    }

    void parse_missingRecommended_computedFromTiles()
    {
        QList<std::pair<QString, QString>> raw = {
            {"name", "Test"},
            {"format", "png"},
        };

        auto result = MBTilesMetadataParser::parse(raw, ZoomRange{2, 8});

        // minzoom and maxzoom should be computed from tiles
        QVERIFY(result.metadata.hasField("minzoom"));
        QVERIFY(result.metadata.hasField("maxzoom"));
        QCOMPARE(result.metadata.value("minzoom").value(), "2");
        QCOMPARE(result.metadata.value("maxzoom").value(), "8");

        // Should have a warning about missing recommended fields
        bool hasWarning = false;
        for (const auto& msg : result.messages) {
            if (msg.level == ValidationMessage::Level::Warning
                && msg.text.contains("recommended"))
                hasWarning = true;
        }
        QVERIFY(hasWarning);
    }

    void parse_zoomMismatch()
    {
        QList<std::pair<QString, QString>> raw = {
            {"name", "Test"},
            {"format", "png"},
            {"bounds", "-180,-85,180,85"},
            {"center", "0,0,5"},
            {"minzoom", "0"},
            {"maxzoom", "10"},
        };

        auto result = MBTilesMetadataParser::parse(raw, ZoomRange{0, 8});

        // Should warn about maxzoom mismatch
        bool hasZoomWarning = false;
        for (const auto& msg : result.messages) {
            if (msg.text.contains("maxzoom") && msg.text.contains("does not match"))
                hasZoomWarning = true;
        }
        QVERIFY(hasZoomWarning);
    }

    void parse_unparseableBounds()
    {
        QList<std::pair<QString, QString>> raw = {
            {"name", "Test"},
            {"format", "png"},
            {"bounds", "not,valid,numbers,here"},
            {"center", "0,0,5"},
            {"minzoom", "0"},
            {"maxzoom", "10"},
        };

        auto result = MBTilesMetadataParser::parse(raw, ZoomRange{0, 10});

        bool hasBoundsError = false;
        for (const auto& msg : result.messages) {
            if (msg.level == ValidationMessage::Level::Error && msg.text.contains("bounds"))
                hasBoundsError = true;
        }
        QVERIFY(hasBoundsError);
    }

    void parse_outOfRangeCoords()
    {
        QList<std::pair<QString, QString>> raw = {
            {"name", "Test"},
            {"format", "png"},
            {"bounds", "-200,-90,200,90"},
            {"center", "0,0,5"},
            {"minzoom", "0"},
            {"maxzoom", "10"},
        };

        auto result = MBTilesMetadataParser::parse(raw, ZoomRange{0, 10});

        // Should have warnings about out-of-range coordinates
        bool hasRangeWarning = false;
        for (const auto& msg : result.messages) {
            if (msg.level == ValidationMessage::Level::Warning && msg.text.contains("range"))
                hasRangeWarning = true;
        }
        QVERIFY(hasRangeWarning);
    }

    void parse_nonStandardFields()
    {
        QList<std::pair<QString, QString>> raw = {
            {"name", "Test"},
            {"format", "png"},
            {"bounds", "-180,-85,180,85"},
            {"center", "0,0,5"},
            {"minzoom", "0"},
            {"maxzoom", "10"},
            {"custom_field", "custom_value"},
            {"another_field", "another_value"},
        };

        auto result = MBTilesMetadataParser::parse(raw, ZoomRange{0, 10});

        auto nonStandard = result.metadata.fieldsByCategory(FieldCategory::NonStandard);
        QCOMPARE(nonStandard.size(), 2);
    }
};

QTEST_APPLESS_MAIN(TestMBTilesMetadataParser)
#include "tst_MBTilesMetadataParser.moc"
