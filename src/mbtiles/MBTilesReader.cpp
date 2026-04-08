#include "mbtiles/MBTilesReader.h"

#include <QMap>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

MBTilesReader::MBTilesReader(const QString& filePath)
    : m_filePath(filePath)
    , m_connectionName("tilepeek_" + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

MBTilesReader::~MBTilesReader()
{
    close();
}

bool MBTilesReader::open()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_filePath);
    m_db.setConnectOptions("QSQLITE_OPEN_READONLY");
    return m_db.open();
}

void MBTilesReader::close()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

MBTilesValidationResult MBTilesReader::validateSchema() const
{
    MBTilesValidationResult result;

    // Validate metadata table
    if (!tableOrViewExists("metadata")) {
        result.errors.append("Missing required table or view: metadata");
    } else {
        QMap<QString, QString> metadataCols;
        metadataCols["name"] = "text";
        metadataCols["value"] = "text";
        result.metadataTableValid =
            validateTableColumns("metadata", metadataCols, result.errors);
    }

    // Validate tiles table
    if (!tableOrViewExists("tiles")) {
        result.errors.append("Missing required table or view: tiles");
    } else {
        QMap<QString, QString> tilesCols;
        tilesCols["zoom_level"] = "integer";
        tilesCols["tile_column"] = "integer";
        tilesCols["tile_row"] = "integer";
        tilesCols["tile_data"] = "blob";
        result.tilesTableValid =
            validateTableColumns("tiles", tilesCols, result.errors);
    }

    return result;
}

QList<std::pair<QString, QString>> MBTilesReader::readRawMetadata() const
{
    QList<std::pair<QString, QString>> metadata;
    QSqlQuery query(m_db);
    if (query.exec("SELECT name, value FROM metadata")) {
        while (query.next()) {
            metadata.append({query.value(0).toString(), query.value(1).toString()});
        }
    }
    return metadata;
}

std::optional<ZoomRange> MBTilesReader::queryZoomRange() const
{
    QSqlQuery query(m_db);
    if (query.exec("SELECT MIN(zoom_level), MAX(zoom_level) FROM tiles") && query.next()) {
        if (query.value(0).isNull())
            return std::nullopt;
        return ZoomRange{query.value(0).toInt(), query.value(1).toInt()};
    }
    return std::nullopt;
}

std::optional<QByteArray> MBTilesReader::readTileData(int zoom, int column, int row) const
{
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?");
    query.addBindValue(zoom);
    query.addBindValue(column);
    query.addBindValue(row);
    if (query.exec() && query.next())
        return query.value(0).toByteArray();
    return std::nullopt;
}

bool MBTilesReader::tableOrViewExists(const QString& name) const
{
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT name FROM sqlite_master WHERE type IN ('table', 'view') AND name = ?");
    query.addBindValue(name);
    return query.exec() && query.next();
}

bool MBTilesReader::validateTableColumns(const QString& table,
                                          const QMap<QString, QString>& expectedColumns,
                                          QStringList& errors) const
{
    QSqlQuery query(m_db);
    if (!query.exec(QString("PRAGMA table_info(%1)").arg(table))) {
        errors.append(QString("Failed to read schema for table: %1").arg(table));
        return false;
    }

    // PRAGMA table_info returns: cid, name, type, notnull, dflt_value, pk
    QMap<QString, QString> foundColumns;
    while (query.next()) {
        QString colName = query.value(1).toString().toLower();
        QString colType = query.value(2).toString().toLower();
        foundColumns[colName] = colType;
    }

    bool valid = true;
    for (auto it = expectedColumns.constBegin(); it != expectedColumns.constEnd(); ++it) {
        if (!foundColumns.contains(it.key())) {
            errors.append(QString("Table '%1' is missing required column: %2")
                              .arg(table, it.key()));
            valid = false;
        } else if (foundColumns[it.key()] != it.value()) {
            errors.append(
                QString("Table '%1' column '%2' has type '%3', expected '%4'")
                    .arg(table, it.key(), foundColumns[it.key()], it.value()));
            valid = false;
        }
    }
    return valid;
}
