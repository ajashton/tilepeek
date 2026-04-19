// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mbtiles/MBTilesMetadataParser.h"
#include "model/TilesetMetadata.h"

#include <QList>
#include <QString>
#include <pmtiles.hpp>

class PMTilesMetadataParser {
    Q_DECLARE_TR_FUNCTIONS(PMTilesMetadataParser)
public:
    struct Result {
        TilesetMetadata metadata;
        QList<ValidationMessage> messages;
    };

    static Result parse(const pmtiles::headerv3& header, const QString& jsonMetadata);

    static QString tileTypeToFormat(uint8_t tileType);
};
