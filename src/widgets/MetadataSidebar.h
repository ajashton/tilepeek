#pragma once

#include "model/TilesetMetadata.h"

#include <QWidget>

class QScrollArea;
class QVBoxLayout;

class MetadataSidebar : public QWidget {
    Q_OBJECT
public:
    explicit MetadataSidebar(QWidget* parent = nullptr);

    void setMetadata(const TilesetMetadata& metadata);
    void clear();

private:
    void addSection(QVBoxLayout* layout, const QList<MetadataField>& fields, bool addSeparator);

    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget = nullptr;
};
