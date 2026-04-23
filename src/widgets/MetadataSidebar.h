// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "mbtiles/MBTilesMetadataParser.h"
#include "model/TilesetMetadata.h"
#include "mvt/FeatureHitTest.h"

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QWidget>

class QLabel;
class QScrollArea;
class QTabWidget;
class QToolButton;
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
    void labeledFieldsChanged(const QHash<QString, QString>& fieldsByLayer);
    void featureIsolated(int index);

private slots:
    void onLayerVisibilityToggled();
    void onLabelFieldToggled(const QString& layerId, const QString& fieldName, bool on);

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
    QMap<QString, QToolButton*> m_layerVisibilityButtons;
    // Keyed by [layerId][fieldName].
    QMap<QString, QMap<QString, QToolButton*>> m_labelFieldButtons;
    QJsonObject m_rawJson;
    int m_inspectTabIndex = -1;
    int m_selectedFeatureIndex = -1;
};
