#pragma once

#include "model/TilesetMetadata.h"

#include <QWidget>

class QScrollArea;
class QVBoxLayout;
struct TileStatistics;

class MetadataSidebar : public QWidget {
    Q_OBJECT
public:
    explicit MetadataSidebar(QWidget* parent = nullptr);

    void setMetadata(const TilesetMetadata& metadata);
    void setStatsPlaceholder();
    void setTileStatistics(const TileStatistics& stats);
    void clear();

private:
    void addSection(QVBoxLayout* layout, const QList<MetadataField>& fields, bool addSeparator);
    void clearStatsSection();

    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QVBoxLayout* m_statsLayout = nullptr;
};
