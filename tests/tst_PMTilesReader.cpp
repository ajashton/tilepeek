#include "pmtiles/PMTilesMetadataParser.h"
#include "pmtiles/PMTilesReader.h"

#include <QTemporaryFile>
#include <QTest>
#include <pmtiles.hpp>

class tst_PMTilesReader : public QObject {
    Q_OBJECT

private:
    // Build a minimal valid PMTiles file with the given tiles and metadata
    QByteArray buildPMTilesFile(
        const std::vector<std::pair<pmtiles::zxy, QByteArray>>& tiles,
        const QString& jsonMetadata = "{}",
        uint8_t tileType = pmtiles::TILETYPE_PNG,
        uint8_t tileCompression = pmtiles::COMPRESSION_NONE)
    {
        // Build directory entries and tile data
        std::vector<pmtiles::entryv3> entries;
        QByteArray tileDataSection;

        for (const auto& [zxy, data] : tiles) {
            uint64_t tileId = pmtiles::zxy_to_tileid(zxy.z, zxy.x, zxy.y);
            entries.push_back({tileId, static_cast<uint64_t>(tileDataSection.size()),
                               static_cast<uint32_t>(data.size()), 1});
            tileDataSection.append(data);
        }

        std::sort(entries.begin(), entries.end(), pmtiles::entryv3_cmp);

        // Serialize directory (uncompressed for simplicity)
        std::string dirBytes = pmtiles::serialize_directory(entries);

        // Build the file layout
        QByteArray metaBytes = jsonMetadata.toUtf8();

        pmtiles::headerv3 header{};
        header.root_dir_offset = 127;
        header.root_dir_bytes = dirBytes.size();
        header.json_metadata_offset = 127 + dirBytes.size();
        header.json_metadata_bytes = metaBytes.size();
        header.leaf_dirs_offset = header.json_metadata_offset + metaBytes.size();
        header.leaf_dirs_bytes = 0;
        header.tile_data_offset = header.leaf_dirs_offset;
        header.tile_data_bytes = tileDataSection.size();
        header.addressed_tiles_count = tiles.size();
        header.tile_entries_count = entries.size();
        header.tile_contents_count = entries.size();
        header.clustered = true;
        header.internal_compression = pmtiles::COMPRESSION_NONE;
        header.tile_compression = tileCompression;
        header.tile_type = tileType;
        header.min_zoom = 0;
        header.max_zoom = 0;
        header.min_lon_e7 = -1800000000;
        header.min_lat_e7 = -850511290;
        header.max_lon_e7 = 1800000000;
        header.max_lat_e7 = 850511290;
        header.center_zoom = 0;
        header.center_lon_e7 = 0;
        header.center_lat_e7 = 0;

        if (!tiles.empty()) {
            uint8_t minZ = 255, maxZ = 0;
            for (const auto& [zxy, _] : tiles) {
                minZ = std::min(minZ, zxy.z);
                maxZ = std::max(maxZ, zxy.z);
            }
            header.min_zoom = minZ;
            header.max_zoom = maxZ;
        }

        std::string headerStr = header.serialize();

        QByteArray fileData;
        fileData.append(headerStr.data(), headerStr.size());
        fileData.append(dirBytes.data(), dirBytes.size());
        fileData.append(metaBytes);
        fileData.append(tileDataSection);

        return fileData;
    }

    QString writeTempFile(const QByteArray& data)
    {
        auto* tmp = new QTemporaryFile(this);
        tmp->setFileTemplate(QDir::tempPath() + "/tilepeek_test_XXXXXX.pmtiles");
        if (!tmp->open())
            return {};

        tmp->write(data);
        tmp->flush();
        return tmp->fileName();
    }

private slots:
    void openValidFile()
    {
        QByteArray tileData("fake png tile data");
        auto fileBytes = buildPMTilesFile(
            {{pmtiles::zxy(0, 0, 0), tileData}});

        auto path = writeTempFile(fileBytes);
        PMTilesReader reader(path);
        QVERIFY(reader.open());
        QCOMPARE(reader.header().tile_type, pmtiles::TILETYPE_PNG);
        QCOMPARE(reader.header().min_zoom, 0);
        QCOMPARE(reader.header().max_zoom, 0);
    }

    void readTileData()
    {
        QByteArray tile0("tile at 0/0/0");
        QByteArray tile1("tile at 1/0/0");
        auto fileBytes = buildPMTilesFile({
            {pmtiles::zxy(0, 0, 0), tile0},
            {pmtiles::zxy(1, 0, 0), tile1},
        });

        auto path = writeTempFile(fileBytes);
        PMTilesReader reader(path);
        QVERIFY(reader.open());

        auto result0 = reader.readTile(0, 0, 0);
        QVERIFY(result0.has_value());
        QCOMPARE(*result0, tile0);

        auto result1 = reader.readTile(1, 0, 0);
        QVERIFY(result1.has_value());
        QCOMPARE(*result1, tile1);

        // Non-existent tile
        auto missing = reader.readTile(2, 0, 0);
        QVERIFY(!missing.has_value());
    }

    void tileSize()
    {
        QByteArray tileData("twelve chars");
        auto fileBytes = buildPMTilesFile(
            {{pmtiles::zxy(0, 0, 0), tileData}});

        auto path = writeTempFile(fileBytes);
        PMTilesReader reader(path);
        QVERIFY(reader.open());

        auto size = reader.tileSize(0, 0, 0);
        QVERIFY(size.has_value());
        QCOMPARE(*size, tileData.size());

        QVERIFY(!reader.tileSize(1, 0, 0).has_value());
    }

    void jsonMetadata()
    {
        QString meta = R"({"name":"test tileset","description":"a test"})";
        auto fileBytes = buildPMTilesFile(
            {{pmtiles::zxy(0, 0, 0), QByteArray("tile")}}, meta);

        auto path = writeTempFile(fileBytes);
        PMTilesReader reader(path);
        QVERIFY(reader.open());

        auto json = reader.readJsonMetadata();
        QVERIFY(json.contains("test tileset"));
    }

    void metadataParser()
    {
        pmtiles::headerv3 header{};
        header.tile_type = pmtiles::TILETYPE_PNG;
        header.min_zoom = 2;
        header.max_zoom = 14;
        header.min_lon_e7 = -734375000;
        header.min_lat_e7 = 408128000;
        header.max_lon_e7 = -734000000;
        header.max_lat_e7 = 408200000;
        header.center_lon_e7 = -734187500;
        header.center_lat_e7 = 408164000;
        header.center_zoom = 10;

        auto [metadata, messages] = PMTilesMetadataParser::parse(header, R"({"name":"NYC"})");

        QCOMPARE(metadata.value("format").value_or(""), "png");
        QCOMPARE(metadata.value("minzoom").value_or(""), "2");
        QCOMPARE(metadata.value("maxzoom").value_or(""), "14");
        QVERIFY(metadata.hasField("bounds"));
        QVERIFY(metadata.hasField("center"));
        QCOMPARE(metadata.value("name").value_or(""), "NYC");
    }

    void tileTypeToFormat()
    {
        QCOMPARE(PMTilesMetadataParser::tileTypeToFormat(pmtiles::TILETYPE_MVT), "pbf");
        QCOMPARE(PMTilesMetadataParser::tileTypeToFormat(pmtiles::TILETYPE_PNG), "png");
        QCOMPARE(PMTilesMetadataParser::tileTypeToFormat(pmtiles::TILETYPE_JPEG), "jpeg");
        QCOMPARE(PMTilesMetadataParser::tileTypeToFormat(pmtiles::TILETYPE_WEBP), "webp");
        QCOMPARE(PMTilesMetadataParser::tileTypeToFormat(pmtiles::TILETYPE_AVIF), "avif");
        QCOMPARE(PMTilesMetadataParser::tileTypeToFormat(pmtiles::TILETYPE_UNKNOWN), "unknown");
    }

    void unsupportedCompression()
    {
        QByteArray tileData("tile");
        auto fileBytes = buildPMTilesFile(
            {{pmtiles::zxy(0, 0, 0), tileData}}, "{}", pmtiles::TILETYPE_PNG);

        // Patch internal_compression to brotli (byte 97)
        fileBytes[97] = pmtiles::COMPRESSION_BROTLI;

        auto path = writeTempFile(fileBytes);
        PMTilesReader reader(path);
        // open() may succeed since we only parse the header
        // but validate() should report error
        if (reader.open()) {
            auto result = reader.validate();
            QVERIFY(!result.valid);
            QVERIFY(!result.errors.isEmpty());
            QVERIFY(result.errors.first().contains("Brotli"));
        }
    }
};

QTEST_MAIN(tst_PMTilesReader)
#include "tst_PMTilesReader.moc"
