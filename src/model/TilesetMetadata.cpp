#include "model/TilesetMetadata.h"

#include <QMap>

FieldCategory categorizeFieldName(const QString& name)
{
    static const QMap<QString, FieldCategory> map = {
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
        {"json", FieldCategory::Standard},
    };
    return map.value(name, FieldCategory::NonStandard);
}

void TilesetMetadata::addField(const QString& name, const QString& value, FieldCategory category)
{
    m_fields.append({name, value, category});
}

void TilesetMetadata::clear()
{
    m_fields.clear();
}

QList<MetadataField> TilesetMetadata::fields() const
{
    return m_fields;
}

QList<MetadataField> TilesetMetadata::fieldsByCategory(FieldCategory category) const
{
    QList<MetadataField> result;
    for (const auto& field : m_fields) {
        if (field.category == category)
            result.append(field);
    }
    return result;
}

std::optional<QString> TilesetMetadata::value(const QString& name) const
{
    for (const auto& field : m_fields) {
        if (field.name == name)
            return field.value;
    }
    return std::nullopt;
}

bool TilesetMetadata::hasField(const QString& name) const
{
    for (const auto& field : m_fields) {
        if (field.name == name)
            return true;
    }
    return false;
}
