#pragma once

#include "mbtiles/MBTilesReader.h"
#include "model/TilesetMetadata.h"

#include <QList>
#include <QString>
#include <optional>

struct ValidationMessage {
    enum class Level { Error, Warning };
    Level level;
    QString text;
};

struct ParsedBounds {
    double left, bottom, right, top;
};

struct ParsedCenter {
    double longitude, latitude, zoom;
};

class MBTilesMetadataParser {
public:
    struct Result {
        TilesetMetadata metadata;
        QList<ValidationMessage> messages;
    };

    static Result parse(const QList<std::pair<QString, QString>>& rawMetadata,
                        std::optional<ZoomRange> tilesZoomRange);

    static std::optional<ParsedBounds> parseBounds(const QString& value);
    static std::optional<ParsedCenter> parseCenter(const QString& value);
    static bool isLongitudeInRange(double x);
    static bool isLatitudeInRange(double y);

    static constexpr double MaxLongitude = 180.0;
    static constexpr double MaxLatitude = 85.051129;

private:
    static FieldCategory categorize(const QString& name);
};
