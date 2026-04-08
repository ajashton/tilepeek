#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <optional>
#include <utility>

struct MBTilesValidationResult {
    bool metadataTableValid = false;
    bool tilesTableValid = false;
    QStringList errors;
};

struct ZoomRange {
    int minZoom;
    int maxZoom;
};

class MBTilesReader {
public:
    explicit MBTilesReader(const QString& filePath);
    ~MBTilesReader();

    MBTilesReader(const MBTilesReader&) = delete;
    MBTilesReader& operator=(const MBTilesReader&) = delete;

    bool open();
    void close();

    QString connectionName() const { return m_connectionName; }

    MBTilesValidationResult validateSchema() const;
    QList<std::pair<QString, QString>> readRawMetadata() const;
    std::optional<ZoomRange> queryZoomRange() const;
    std::optional<QByteArray> readTileData(int zoom, int column, int row) const;

private:
    bool tableOrViewExists(const QString& name) const;
    bool validateTableColumns(const QString& table,
                              const QMap<QString, QString>& expectedColumns,
                              QStringList& errors) const;

    QString m_filePath;
    QString m_connectionName;
    QSqlDatabase m_db;
};
