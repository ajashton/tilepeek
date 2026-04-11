#include "mbtiles/VectorMetadataParser.h"

#include <QJsonArray>
#include <QJsonDocument>

VectorMetadataResult VectorMetadataParser::parse(const QString& jsonString)
{
    VectorMetadataResult result;

    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);
    if (doc.isNull()) {
        result.messages.append({ValidationMessage::Level::Error,
                                "Invalid JSON: " + parseError.errorString(), "json"});
        return result;
    }

    if (!doc.isObject()) {
        result.messages.append(
            {ValidationMessage::Level::Error, "Must be a JSON object", "json"});
        return result;
    }

    QJsonObject obj = doc.object();
    VectorMetadata meta;
    meta.rawJson = obj;

    // Check for required vector_layers
    if (!obj.contains("vector_layers")) {
        result.messages.append({ValidationMessage::Level::Error,
                                "Missing required 'vector_layers'", "json"});
        return result;
    }

    auto layersVal = obj["vector_layers"];
    if (!layersVal.isArray()) {
        result.messages.append(
            {ValidationMessage::Level::Error, "'vector_layers' must be a JSON array", "json"});
        return result;
    }

    for (const auto& layerVal : layersVal.toArray()) {
        if (!layerVal.isObject())
            continue;
        auto layerObj = layerVal.toObject();

        VectorLayerInfo info;
        info.id = layerObj["id"].toString();
        info.description = layerObj["description"].toString();
        info.minzoom = layerObj["minzoom"].toInt(0);
        info.maxzoom = layerObj["maxzoom"].toInt(0);

        if (layerObj.contains("fields") && layerObj["fields"].isObject()) {
            auto fieldsObj = layerObj["fields"].toObject();
            for (auto it = fieldsObj.begin(); it != fieldsObj.end(); ++it)
                info.fields[it.key()] = it.value().toString();
        }

        meta.vectorLayers.append(info);
    }

    // Check for optional tilestats
    if (obj.contains("tilestats")) {
        meta.tilestats = obj["tilestats"].toObject();
        meta.hasTilestats = true;
    }

    result.metadata = std::move(meta);
    return result;
}
