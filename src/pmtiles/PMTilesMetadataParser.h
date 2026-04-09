#pragma once

#include "mbtiles/MBTilesMetadataParser.h"
#include "model/TilesetMetadata.h"

#include <QList>
#include <QString>
#include <pmtiles.hpp>

class PMTilesMetadataParser {
public:
    struct Result {
        TilesetMetadata metadata;
        QList<ValidationMessage> messages;
    };

    static Result parse(const pmtiles::headerv3& header, const QString& jsonMetadata);

    static QString tileTypeToFormat(uint8_t tileType);
};
