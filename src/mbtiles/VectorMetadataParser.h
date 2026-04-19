// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mbtiles/MBTilesMetadataParser.h"

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <optional>

struct VectorLayerInfo {
    QString id;
    QString description;
    int minzoom = 0;
    int maxzoom = 0;
    QMap<QString, QString> fields;
};

struct VectorMetadata {
    QList<VectorLayerInfo> vectorLayers;
    QJsonObject tilestats;
    QJsonObject rawJson;
    bool hasTilestats = false;
};

struct VectorMetadataResult {
    std::optional<VectorMetadata> metadata;
    QList<ValidationMessage> messages;
};

class VectorMetadataParser {
    Q_DECLARE_TR_FUNCTIONS(VectorMetadataParser)
public:
    static VectorMetadataResult parse(const QString& jsonString);
};
