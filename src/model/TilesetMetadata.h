#pragma once

#include <QList>
#include <QString>
#include <optional>

enum class FieldCategory {
    Required,
    Recommended,
    Standard,
    NonStandard,
};

struct MetadataField {
    QString name;
    QString value;
    FieldCategory category;
};

FieldCategory categorizeFieldName(const QString& name);

class TilesetMetadata {
public:
    void addField(const QString& name, const QString& value, FieldCategory category);
    void clear();

    QList<MetadataField> fields() const;
    QList<MetadataField> fieldsByCategory(FieldCategory category) const;

    std::optional<QString> value(const QString& name) const;
    bool hasField(const QString& name) const;

private:
    QList<MetadataField> m_fields;
};
