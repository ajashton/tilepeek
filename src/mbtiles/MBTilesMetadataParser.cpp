#include "mbtiles/MBTilesMetadataParser.h"

#include <QMap>

static const QMap<QString, FieldCategory> s_fieldCategories = {
    {"name", FieldCategory::Required},
    {"format", FieldCategory::Required},
    {"bounds", FieldCategory::Recommended},
    {"center", FieldCategory::Recommended},
    {"minzoom", FieldCategory::Recommended},
    {"maxzoom", FieldCategory::Recommended},
    {"attribution", FieldCategory::Standard},
    {"description", FieldCategory::Standard},
    {"type", FieldCategory::Standard},
    {"version", FieldCategory::Standard},
};

FieldCategory MBTilesMetadataParser::categorize(const QString& name)
{
    return s_fieldCategories.value(name, FieldCategory::NonStandard);
}

std::optional<ParsedBounds> MBTilesMetadataParser::parseBounds(const QString& value)
{
    auto parts = value.split(',');
    if (parts.size() != 4)
        return std::nullopt;

    bool ok1, ok2, ok3, ok4;
    double left = parts[0].trimmed().toDouble(&ok1);
    double bottom = parts[1].trimmed().toDouble(&ok2);
    double right = parts[2].trimmed().toDouble(&ok3);
    double top = parts[3].trimmed().toDouble(&ok4);

    if (!ok1 || !ok2 || !ok3 || !ok4)
        return std::nullopt;

    return ParsedBounds{left, bottom, right, top};
}

std::optional<ParsedCenter> MBTilesMetadataParser::parseCenter(const QString& value)
{
    auto parts = value.split(',');
    if (parts.size() != 3)
        return std::nullopt;

    bool ok1, ok2, ok3;
    double lon = parts[0].trimmed().toDouble(&ok1);
    double lat = parts[1].trimmed().toDouble(&ok2);
    double zoom = parts[2].trimmed().toDouble(&ok3);

    if (!ok1 || !ok2 || !ok3)
        return std::nullopt;

    return ParsedCenter{lon, lat, zoom};
}

bool MBTilesMetadataParser::isLongitudeInRange(double x)
{
    return x >= -MaxLongitude && x <= MaxLongitude;
}

bool MBTilesMetadataParser::isLatitudeInRange(double y)
{
    return y >= -MaxLatitude && y <= MaxLatitude;
}

MBTilesMetadataParser::Result
MBTilesMetadataParser::parse(const QList<std::pair<QString, QString>>& rawMetadata,
                             std::optional<ZoomRange> tilesZoomRange)
{
    Result result;

    // Categorize and add all fields
    for (const auto& [key, value] : rawMetadata) {
        result.metadata.addField(key, value, categorize(key));
    }

    // Check required fields
    if (!result.metadata.hasField("name")) {
        result.messages.append(
            {ValidationMessage::Level::Error, "Missing required metadata field: name"});
    }
    if (!result.metadata.hasField("format")) {
        result.messages.append(
            {ValidationMessage::Level::Error, "Missing required metadata field: format"});
    }

    // Validate bounds
    auto boundsVal = result.metadata.value("bounds");
    if (boundsVal) {
        auto bounds = parseBounds(*boundsVal);
        if (!bounds) {
            result.messages.append({ValidationMessage::Level::Error,
                                    "Cannot parse 'bounds' as comma-separated numbers"});
        } else {
            if (!isLongitudeInRange(bounds->left) || !isLongitudeInRange(bounds->right)) {
                result.messages.append(
                    {ValidationMessage::Level::Warning,
                     "Bounds longitude values are outside valid range (±180.0)"});
            }
            if (!isLatitudeInRange(bounds->bottom) || !isLatitudeInRange(bounds->top)) {
                result.messages.append(
                    {ValidationMessage::Level::Warning,
                     "Bounds latitude values are outside valid range (±85.051129)"});
            }
        }
    }

    // Validate center
    auto centerVal = result.metadata.value("center");
    if (centerVal) {
        auto center = parseCenter(*centerVal);
        if (!center) {
            result.messages.append({ValidationMessage::Level::Error,
                                    "Cannot parse 'center' as comma-separated numbers"});
        } else {
            if (!isLongitudeInRange(center->longitude)) {
                result.messages.append(
                    {ValidationMessage::Level::Warning,
                     "Center longitude is outside valid range (±180.0)"});
            }
            if (!isLatitudeInRange(center->latitude)) {
                result.messages.append(
                    {ValidationMessage::Level::Warning,
                     "Center latitude is outside valid range (±85.051129)"});
            }
        }
    }

    // Validate and compute minzoom/maxzoom
    auto minzoomVal = result.metadata.value("minzoom");
    auto maxzoomVal = result.metadata.value("maxzoom");
    bool missingRecommended = false;

    if (!minzoomVal) {
        missingRecommended = true;
        if (tilesZoomRange) {
            result.metadata.addField(
                "minzoom", QString::number(tilesZoomRange->minZoom), FieldCategory::Recommended);
        }
    }
    if (!maxzoomVal) {
        missingRecommended = true;
        if (tilesZoomRange) {
            result.metadata.addField(
                "maxzoom", QString::number(tilesZoomRange->maxZoom), FieldCategory::Recommended);
        }
    }

    if (!boundsVal || !centerVal || !minzoomVal || !maxzoomVal) {
        QStringList missing;
        if (!boundsVal)
            missing.append("bounds");
        if (!centerVal)
            missing.append("center");
        if (!minzoomVal)
            missing.append("minzoom");
        if (!maxzoomVal)
            missing.append("maxzoom");
        result.messages.append(
            {ValidationMessage::Level::Warning,
             "Missing recommended metadata fields: " + missing.join(", ")});
    }

    // Check minzoom/maxzoom against tiles table
    if (tilesZoomRange) {
        if (minzoomVal) {
            bool ok;
            int minzoom = minzoomVal->toInt(&ok);
            if (ok && minzoom != tilesZoomRange->minZoom) {
                result.messages.append(
                    {ValidationMessage::Level::Warning,
                     QString("Metadata minzoom (%1) does not match tiles table (%2)")
                         .arg(minzoom)
                         .arg(tilesZoomRange->minZoom)});
            }
        }
        if (maxzoomVal) {
            bool ok;
            int maxzoom = maxzoomVal->toInt(&ok);
            if (ok && maxzoom != tilesZoomRange->maxZoom) {
                result.messages.append(
                    {ValidationMessage::Level::Warning,
                     QString("Metadata maxzoom (%1) does not match tiles table (%2)")
                         .arg(maxzoom)
                         .arg(tilesZoomRange->maxZoom)});
            }
        }
    }

    return result;
}
