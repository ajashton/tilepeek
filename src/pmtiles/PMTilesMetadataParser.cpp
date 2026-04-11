#include "pmtiles/PMTilesMetadataParser.h"

#include <QJsonDocument>
#include <QJsonObject>

QString PMTilesMetadataParser::tileTypeToFormat(uint8_t tileType)
{
    switch (tileType) {
    case pmtiles::TILETYPE_MVT:
    case pmtiles::TILETYPE_MLT:
        return "pbf";
    case pmtiles::TILETYPE_PNG:
        return "png";
    case pmtiles::TILETYPE_JPEG:
        return "jpeg";
    case pmtiles::TILETYPE_WEBP:
        return "webp";
    case pmtiles::TILETYPE_AVIF:
        return "avif";
    default:
        return "unknown";
    }
}

PMTilesMetadataParser::Result PMTilesMetadataParser::parse(
    const pmtiles::headerv3& header, const QString& jsonMetadata)
{
    Result result;
    auto& meta = result.metadata;

    // Format from header tile type
    QString format = tileTypeToFormat(header.tile_type);
    meta.addField("format", format, FieldCategory::Required);

    if (format == "unknown")
        result.messages.append({ValidationMessage::Level::Warning,
                                "Unknown tile type in header", "format"});

    // Zoom range from header
    meta.addField("minzoom", QString::number(header.min_zoom), FieldCategory::Recommended);
    meta.addField("maxzoom", QString::number(header.max_zoom), FieldCategory::Recommended);

    // Bounds from header (stored as int32 * 1e-7)
    double minLon = header.min_lon_e7 / 1e7;
    double minLat = header.min_lat_e7 / 1e7;
    double maxLon = header.max_lon_e7 / 1e7;
    double maxLat = header.max_lat_e7 / 1e7;

    if (minLon != 0.0 || minLat != 0.0 || maxLon != 0.0 || maxLat != 0.0) {
        meta.addField("bounds",
                       QString("%1,%2,%3,%4").arg(minLon).arg(minLat).arg(maxLon).arg(maxLat),
                       FieldCategory::Recommended);
    }

    // Center from header
    double centerLon = header.center_lon_e7 / 1e7;
    double centerLat = header.center_lat_e7 / 1e7;

    if (centerLon != 0.0 || centerLat != 0.0) {
        meta.addField("center",
                       QString("%1,%2,%3").arg(centerLon).arg(centerLat).arg(header.center_zoom),
                       FieldCategory::Recommended);
    }

    // Parse JSON metadata for additional fields
    if (!jsonMetadata.isEmpty()) {
        QJsonParseError parseError;
        auto doc = QJsonDocument::fromJson(jsonMetadata.toUtf8(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            result.messages.append({ValidationMessage::Level::Error,
                                    "Failed to parse JSON metadata: " + parseError.errorString()});
        } else if (doc.isObject()) {
            auto obj = doc.object();

            // Standard optional fields from the PMTiles spec
            static constexpr const char* standardFields[] = {
                "name", "description", "attribution", "type", "version"};

            for (const char* key : standardFields) {
                if (obj.contains(key)) {
                    QString val = obj.value(key).toString();
                    if (!val.isEmpty())
                        meta.addField(key, val, FieldCategory::Standard);
                }
            }

            // Store the full JSON for vector tiles (vector_layers parsing)
            if (format == "pbf" && obj.contains("vector_layers")) {
                meta.addField("json", jsonMetadata, FieldCategory::Standard);
            }
        }
    } else if (format == "pbf") {
        result.messages.append({ValidationMessage::Level::Warning,
                                "Missing JSON metadata for vector tile format"});
    }

    // Name field is recommended
    if (!meta.hasField("name"))
        result.messages.append({ValidationMessage::Level::Warning,
                                "Missing recommended field", "name"});

    return result;
}
