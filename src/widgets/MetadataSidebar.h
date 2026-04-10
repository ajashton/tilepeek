#pragma once

#include "model/TilesetMetadata.h"

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QWidget>

class QCheckBox;
class QLabel;
class QScrollArea;
class QTabWidget;
class QVBoxLayout;
struct TileStatistics;
struct VectorLayerInfo;
struct VectorMetadata;

class MetadataSidebar : public QWidget {
    Q_OBJECT
public:
    explicit MetadataSidebar(QWidget* parent = nullptr);

    void setMetadata(const TilesetMetadata& metadata);
    void setVectorMetadata(const TilesetMetadata& metadata, const VectorMetadata& vectorMeta,
                           const QList<QColor>& layerColors);
    void setStatsPlaceholder();
    void setTileStatistics(const TileStatistics& stats);
    void clear();

signals:
    void layerVisibilityChanged(const QSet<QString>& hiddenLayers);

private slots:
    void onLayerCheckboxToggled();

private:
    void addSection(QVBoxLayout* layout, const QList<MetadataField>& fields, bool addSeparator);
    void clearStatsSection();
    QWidget* buildMetadataWidget(const TilesetMetadata& metadata, bool skipJson);
    QWidget* buildLayersWidget(const QList<VectorLayerInfo>& layers, const QList<QColor>& layerColors);
    QWidget* buildTilestatsWidget(const QJsonObject& tilestats);
    void showJsonWindow();

    QVBoxLayout* m_outerLayout = nullptr;
    QLabel* m_header = nullptr;

    // Raster mode
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QVBoxLayout* m_statsLayout = nullptr;

    // Vector mode (tabbed)
    QTabWidget* m_tabWidget = nullptr;
    QMap<QString, QCheckBox*> m_layerCheckboxes;
    QJsonObject m_rawJson;
};
