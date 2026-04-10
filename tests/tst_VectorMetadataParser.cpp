#include "mbtiles/VectorMetadataParser.h"

#include <QTest>

class TestVectorMetadataParser : public QObject {
    Q_OBJECT

private slots:
    void validMinimal()
    {
        auto result = VectorMetadataParser::parse(R"({
            "vector_layers": [
                {"id": "roads", "fields": {"name": "String", "type": "Number"}}
            ]
        })");

        QVERIFY(result.metadata.has_value());
        QCOMPARE(result.metadata->vectorLayers.size(), 1);
        QCOMPARE(result.metadata->vectorLayers[0].id, "roads");
        QCOMPARE(result.metadata->vectorLayers[0].fields.size(), 2);
        QCOMPARE(result.metadata->vectorLayers[0].fields["name"], "String");
        QVERIFY(!result.metadata->hasTilestats);

        // No errors or warnings for minimal valid JSON
        bool hasErrors = false;
        for (const auto& msg : result.messages)
            if (msg.level == ValidationMessage::Level::Error)
                hasErrors = true;
        QVERIFY(!hasErrors);
    }

    void validWithTilestats()
    {
        auto result = VectorMetadataParser::parse(R"({
            "vector_layers": [{"id": "water"}],
            "tilestats": {"layers": []}
        })");

        QVERIFY(result.metadata.has_value());
        QVERIFY(result.metadata->hasTilestats);
    }

    void missingVectorLayers()
    {
        auto result = VectorMetadataParser::parse(R"({"other": "stuff"})");

        QVERIFY(!result.metadata.has_value());
        QVERIFY(!result.messages.isEmpty());
        QCOMPARE(result.messages[0].level, ValidationMessage::Level::Error);
    }

    void invalidJson()
    {
        auto result = VectorMetadataParser::parse("not json at all");

        QVERIFY(!result.metadata.has_value());
        QVERIFY(!result.messages.isEmpty());
        QCOMPARE(result.messages[0].level, ValidationMessage::Level::Error);
    }

    void extraTopLevelKeys()
    {
        auto result = VectorMetadataParser::parse(R"({
            "vector_layers": [{"id": "test"}],
            "custom_key": "value",
            "another": 42
        })");

        QVERIFY(result.metadata.has_value());
        // Extra top-level keys should be silently accepted — neither MBTiles
        // nor PMTiles specs disallow unspecified keys.
        for (const auto& msg : result.messages)
            QVERIFY(msg.level != ValidationMessage::Level::Warning);
    }

    void multipleLayersWithDetails()
    {
        auto result = VectorMetadataParser::parse(R"({
            "vector_layers": [
                {"id": "roads", "description": "Road network", "minzoom": 4, "maxzoom": 14},
                {"id": "buildings", "minzoom": 13, "maxzoom": 14, "fields": {"height": "Number"}}
            ]
        })");

        QVERIFY(result.metadata.has_value());
        QCOMPARE(result.metadata->vectorLayers.size(), 2);

        QCOMPARE(result.metadata->vectorLayers[0].id, "roads");
        QCOMPARE(result.metadata->vectorLayers[0].description, "Road network");
        QCOMPARE(result.metadata->vectorLayers[0].minzoom, 4);
        QCOMPARE(result.metadata->vectorLayers[0].maxzoom, 14);

        QCOMPARE(result.metadata->vectorLayers[1].id, "buildings");
        QCOMPARE(result.metadata->vectorLayers[1].fields["height"], "Number");
    }

    void rawJsonPreserved()
    {
        QString json = R"({"vector_layers": [{"id": "test"}]})";
        auto result = VectorMetadataParser::parse(json);

        QVERIFY(result.metadata.has_value());
        QVERIFY(result.metadata->rawJson.contains("vector_layers"));
    }
};

QTEST_GUILESS_MAIN(TestVectorMetadataParser)
#include "tst_VectorMetadataParser.moc"
