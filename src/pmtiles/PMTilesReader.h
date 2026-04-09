#pragma once

#include "map/TileSource.h"

#include <QFile>
#include <QString>
#include <QStringList>
#include <optional>
#include <pmtiles.hpp>

struct PMTilesValidationResult {
    bool valid = false;
    QStringList errors;
};

class PMTilesReader : public TileSource {
public:
    explicit PMTilesReader(const QString& filePath);
    ~PMTilesReader() override;

    PMTilesReader(const PMTilesReader&) = delete;
    PMTilesReader& operator=(const PMTilesReader&) = delete;

    bool open();
    void close();

    const pmtiles::headerv3& header() const { return m_header; }
    PMTilesValidationResult validate() const;
    QString readJsonMetadata() const;

    // TileSource interface
    std::optional<QByteArray> readTile(int zoom, int x, int y) override;
    std::optional<int> tileSize(int zoom, int x, int y) override;

private:
    std::string decompress(const std::string& data, uint8_t compression) const;

    QString m_filePath;
    QFile m_file;
    const char* m_mappedData = nullptr;
    qint64 m_fileSize = 0;
    pmtiles::headerv3 m_header{};
};
