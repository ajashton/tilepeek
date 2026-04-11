#pragma once

#include "mbtiles/MBTilesMetadataParser.h"
#include "model/TilesetMetadata.h"
#include "mvt/FeatureHitTest.h"

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

    void setMetadata(const TilesetMetadata& metadata,
                     const QList<ValidationMessage>& messages = {});
    void setVectorMetadata(const TilesetMetadata& metadata, const VectorMetadata& vectorMeta,
                           const QList<QColor>& layerColors,
                           const QList<ValidationMessage>& messages = {});
    void setStatsPlaceholder();
    void setTileStatistics(const TileStatistics& stats);
    void setInspectResults(const QList<mvt::HitTestResult>& results);
    void clearInspectResults();
    void clear();

signals:
    void layerVisibilityChanged(const QSet<QString>& hiddenLayers);
    void featureIsolated(int index);

private slots:
    void onLayerCheckboxToggled();

private:
    void addSection(QVBoxLayout* layout, const QList<MetadataField>& fields,
                    const QList<MetadataField>& missingFields, bool addSeparator,
                    const QList<ValidationMessage>& messages);
    void addGeneralWarnings(QVBoxLayout* layout, const QList<ValidationMessage>& messages,
                            const QSet<QString>& visibleFields);
    void clearStatsSection();
    QWidget* buildMetadataWidget(const TilesetMetadata& metadata, bool skipJson,
                                 const QList<ValidationMessage>& messages);
    QWidget* buildLayersWidget(const QList<VectorLayerInfo>& layers, const QList<QColor>& layerColors);
    QWidget* buildTilestatsWidget(const QJsonObject& tilestats);
    QWidget* buildInspectWidget(const QList<mvt::HitTestResult>& results);
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
    int m_inspectTabIndex = -1;
    int m_selectedFeatureIndex = -1;
};
