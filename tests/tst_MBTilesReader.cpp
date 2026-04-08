#include "mbtiles/MBTilesReader.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

class TestMBTilesReader : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    QString createTestDb(const QString& name, const QStringList& setupQueries)
    {
        QString path = m_tempDir.filePath(name);
        QString connName = "test_setup_" + name;
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(path);
            if (!db.open()) {
                qWarning() << "Failed to create test DB:" << db.lastError().text();
                return {};
            }

            QSqlQuery query(db);
            for (const auto& sql : setupQueries) {
                if (!query.exec(sql)) {
                    qWarning() << "SQL failed:" << sql << "—" << query.lastError().text();
                    db.close();
                    return {};
                }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
        return path;
    }

    QString createValidMBTiles(const QString& name)
    {
        return createTestDb(name,
                            {"CREATE TABLE metadata (name text, value text)",
                             "INSERT INTO metadata VALUES ('name', 'Test Tileset')",
                             "INSERT INTO metadata VALUES ('format', 'png')",
                             "INSERT INTO metadata VALUES ('bounds', '-180,-85,180,85')",
                             "INSERT INTO metadata VALUES ('center', '0,0,5')",
                             "INSERT INTO metadata VALUES ('minzoom', '0')",
                             "INSERT INTO metadata VALUES ('maxzoom', '5')",
                             "CREATE TABLE tiles (zoom_level integer, tile_column integer, "
                             "tile_row integer, tile_data blob)",
                             "INSERT INTO tiles VALUES (0, 0, 0, X'89504E47')",
                             "INSERT INTO tiles VALUES (5, 10, 20, X'89504E47')"});
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
    }

    void validSchema()
    {
        auto path = createValidMBTiles("valid.mbtiles");
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto result = reader.validateSchema();
        QVERIFY(result.metadataTableValid);
        QVERIFY(result.tilesTableValid);
        QVERIFY(result.errors.isEmpty());
    }

    void missingMetadataTable()
    {
        auto path = createTestDb("no_metadata.mbtiles",
                                 {"CREATE TABLE tiles (zoom_level integer, tile_column integer, "
                                  "tile_row integer, tile_data blob)"});
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto result = reader.validateSchema();
        QVERIFY(!result.metadataTableValid);
        QVERIFY(result.tilesTableValid);
        QVERIFY(!result.errors.isEmpty());
    }

    void missingTilesTable()
    {
        auto path =
            createTestDb("no_tiles.mbtiles", {"CREATE TABLE metadata (name text, value text)"});
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto result = reader.validateSchema();
        QVERIFY(result.metadataTableValid);
        QVERIFY(!result.tilesTableValid);
    }

    void wrongMetadataColumns()
    {
        auto path = createTestDb("wrong_meta.mbtiles",
                                 {"CREATE TABLE metadata (key text, val text)",
                                  "CREATE TABLE tiles (zoom_level integer, tile_column integer, "
                                  "tile_row integer, tile_data blob)"});
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto result = reader.validateSchema();
        QVERIFY(!result.metadataTableValid);
        QVERIFY(result.tilesTableValid);
    }

    void viewsInsteadOfTables()
    {
        auto path = createTestDb(
            "views.mbtiles",
            {"CREATE TABLE _metadata (name text, value text)",
             "CREATE VIEW metadata AS SELECT name, value FROM _metadata",
             "CREATE TABLE _tiles (zoom_level integer, tile_column integer, "
             "tile_row integer, tile_data blob)",
             "CREATE VIEW tiles AS SELECT zoom_level, tile_column, tile_row, tile_data FROM "
             "_tiles"});
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto result = reader.validateSchema();
        QVERIFY(result.metadataTableValid);
        QVERIFY(result.tilesTableValid);
        QVERIFY(result.errors.isEmpty());
    }

    void readRawMetadata()
    {
        auto path = createValidMBTiles("read_meta.mbtiles");
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto metadata = reader.readRawMetadata();
        QVERIFY(metadata.size() >= 6);

        bool foundName = false;
        for (const auto& [key, value] : metadata) {
            if (key == "name") {
                QCOMPARE(value, "Test Tileset");
                foundName = true;
            }
        }
        QVERIFY(foundName);
    }

    void queryZoomRange()
    {
        auto path = createValidMBTiles("zoom_range.mbtiles");
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto range = reader.queryZoomRange();
        QVERIFY(range.has_value());
        QCOMPARE(range->minZoom, 0);
        QCOMPARE(range->maxZoom, 5);
    }

    void queryZoomRange_emptyTiles()
    {
        auto path = createTestDb("empty_tiles.mbtiles",
                                 {"CREATE TABLE metadata (name text, value text)",
                                  "CREATE TABLE tiles (zoom_level integer, tile_column integer, "
                                  "tile_row integer, tile_data blob)"});
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto range = reader.queryZoomRange();
        QVERIFY(!range.has_value());
    }

    void openNonexistentFile()
    {
        MBTilesReader reader("/nonexistent/path/file.mbtiles");
        QVERIFY(!reader.open());
    }

    void readTileData()
    {
        auto path = createValidMBTiles("read_tile.mbtiles");
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        // The valid test DB has a tile at (0, 0, 0) with data X'89504E47'
        auto data = reader.readTileData(0, 0, 0);
        QVERIFY(data.has_value());
        QCOMPARE(data->toHex(), QByteArray("89504e47"));
    }

    void readTileData_missing()
    {
        auto path = createValidMBTiles("read_tile_missing.mbtiles");
        QVERIFY(!path.isEmpty());

        MBTilesReader reader(path);
        QVERIFY(reader.open());

        auto data = reader.readTileData(3, 99, 99);
        QVERIFY(!data.has_value());
    }
};

QTEST_GUILESS_MAIN(TestMBTilesReader)
#include "tst_MBTilesReader.moc"
