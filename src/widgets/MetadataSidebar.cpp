#include "widgets/MetadataSidebar.h"
#include "mbtiles/VectorMetadataParser.h"
#include "model/TileStatistics.h"
#include "util/FormatUtils.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

MetadataSidebar::MetadataSidebar(QWidget* parent)
    : QWidget(parent)
{
    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setContentsMargins(0, 0, 0, 0);

    m_header = new QLabel("Metadata", this);
    m_header->setStyleSheet("font-weight: bold; font-size: 14px; padding: 8px;");
    m_outerLayout->addWidget(m_header);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_outerLayout->addWidget(m_scrollArea);

    setMinimumWidth(260);
    setMaximumWidth(400);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

void MetadataSidebar::clear()
{
    if (m_contentWidget) {
        delete m_contentWidget;
        m_contentWidget = nullptr;
        m_contentLayout = nullptr;
        m_statsLayout = nullptr;
    }
    if (m_tabWidget) {
        delete m_tabWidget;
        m_tabWidget = nullptr;
    }
    if (!m_scrollArea->parent()) {
        // If scrollArea was removed from layout for tab mode, re-add it
    }
    m_layerCheckboxes.clear();
    m_scrollArea->setWidget(nullptr);
    m_scrollArea->show();
    m_header->show();
}

void MetadataSidebar::setMetadata(const TilesetMetadata& metadata)
{
    clear();

    m_contentWidget = buildMetadataWidget(metadata, false);
    m_contentLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());

    // Reserve space for stats section
    m_statsLayout = new QVBoxLayout;
    m_contentLayout->addLayout(m_statsLayout);
    m_contentLayout->addStretch();

    m_scrollArea->setWidget(m_contentWidget);
}

void MetadataSidebar::setVectorMetadata(const TilesetMetadata& metadata,
                                         const VectorMetadata& vectorMeta)
{
    clear();

    // Hide raster-mode widgets
    m_scrollArea->hide();
    m_header->hide();

    // Create tabbed widget
    m_tabWidget = new QTabWidget(this);
    m_outerLayout->addWidget(m_tabWidget);

    // Metadata tab (skips the json field)
    auto* metaWidget = buildMetadataWidget(metadata, true);
    m_contentLayout = qobject_cast<QVBoxLayout*>(metaWidget->layout());
    m_statsLayout = new QVBoxLayout;
    m_contentLayout->addLayout(m_statsLayout);
    m_contentLayout->addStretch();

    auto* metaScroll = new QScrollArea;
    metaScroll->setWidgetResizable(true);
    metaScroll->setFrameShape(QFrame::NoFrame);
    metaScroll->setWidget(metaWidget);
    m_contentWidget = metaWidget;
    m_tabWidget->addTab(metaScroll, "Metadata");

    // Layers tab
    auto* layersWidget = buildLayersWidget(vectorMeta.vectorLayers);
    auto* layersScroll = new QScrollArea;
    layersScroll->setWidgetResizable(true);
    layersScroll->setFrameShape(QFrame::NoFrame);
    layersScroll->setWidget(layersWidget);
    m_tabWidget->addTab(layersScroll, "Layers");

    // Tilestats tab (only if present)
    if (vectorMeta.hasTilestats) {
        auto* statsWidget = buildTilestatsWidget(vectorMeta.tilestats);
        auto* statsScroll = new QScrollArea;
        statsScroll->setWidgetResizable(true);
        statsScroll->setFrameShape(QFrame::NoFrame);
        statsScroll->setWidget(statsWidget);
        m_tabWidget->addTab(statsScroll, "Stats");
    }

    // Raw JSON tab
    auto* jsonWidget = buildRawJsonWidget(vectorMeta.rawJson);
    m_tabWidget->addTab(jsonWidget, "Raw JSON");
}

QWidget* MetadataSidebar::buildMetadataWidget(const TilesetMetadata& metadata, bool skipJson)
{
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 4, 8, 8);

    static constexpr FieldCategory categories[] = {
        FieldCategory::Required,
        FieldCategory::Recommended,
        FieldCategory::Standard,
        FieldCategory::NonStandard,
    };

    bool firstSection = true;
    for (auto category : categories) {
        auto fields = metadata.fieldsByCategory(category);
        if (skipJson) {
            fields.removeIf([](const MetadataField& f) { return f.name == "json"; });
        }
        if (fields.isEmpty())
            continue;
        addSection(layout, fields, !firstSection);
        firstSection = false;
    }

    return widget;
}

QWidget* MetadataSidebar::buildLayersWidget(const QList<VectorLayerInfo>& layers)
{
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 4, 8, 8);

    m_layerCheckboxes.clear();

    for (int i = 0; i < layers.size(); ++i) {
        const auto& layer = layers[i];

        if (i > 0) {
            auto* line = new QFrame;
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            layout->addWidget(line);
        }

        auto* checkbox = new QCheckBox(layer.id);
        checkbox->setChecked(true);
        checkbox->setStyleSheet("font-weight: bold;");
        connect(checkbox, &QCheckBox::toggled, this, &MetadataSidebar::onLayerCheckboxToggled);
        layout->addWidget(checkbox);
        m_layerCheckboxes[layer.id] = checkbox;

        auto* form = new QFormLayout;
        form->setContentsMargins(16, 2, 0, 4);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(4);

        if (!layer.description.isEmpty())
            form->addRow("Description", new QLabel(layer.description));

        form->addRow("Zoom", new QLabel(QString("%1\xe2\x80\x93%2").arg(layer.minzoom).arg(layer.maxzoom)));

        if (!layer.fields.isEmpty()) {
            QStringList fieldList;
            for (auto it = layer.fields.constBegin(); it != layer.fields.constEnd(); ++it)
                fieldList << QString("%1 (%2)").arg(it.key(), it.value());

            auto* fieldsLabel = new QLabel(fieldList.join("\n"));
            fieldsLabel->setWordWrap(true);
            fieldsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            form->addRow("Fields", fieldsLabel);
        }

        layout->addLayout(form);
    }

    layout->addStretch();
    return widget;
}

void MetadataSidebar::onLayerCheckboxToggled()
{
    QSet<QString> hidden;
    for (auto it = m_layerCheckboxes.constBegin(); it != m_layerCheckboxes.constEnd(); ++it) {
        if (!it.value()->isChecked())
            hidden.insert(it.key());
    }
    emit layerVisibilityChanged(hidden);
}

QWidget* MetadataSidebar::buildTilestatsWidget(const QJsonObject& tilestats)
{
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 4, 8, 8);

    QJsonDocument doc(tilestats);
    auto* text = new QPlainTextEdit(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    text->setReadOnly(true);
    QFont mono("monospace", 9);
    mono.setStyleHint(QFont::Monospace);
    text->setFont(mono);
    layout->addWidget(text);

    return widget;
}

QWidget* MetadataSidebar::buildRawJsonWidget(const QJsonObject& json)
{
    QJsonDocument doc(json);
    auto* text = new QPlainTextEdit(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    text->setReadOnly(true);
    QFont mono("monospace", 9);
    mono.setStyleHint(QFont::Monospace);
    text->setFont(mono);
    return text;
}

void MetadataSidebar::addSection(QVBoxLayout* layout, const QList<MetadataField>& fields,
                                  bool addSeparator)
{
    if (addSeparator) {
        auto* line = new QFrame;
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line);
    }

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 4, 0, 4);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    for (const auto& field : fields) {
        auto* nameLabel = new QLabel(field.name);
        nameLabel->setStyleSheet("font-weight: bold; color: #555;");

        auto* valueLabel = new QLabel(field.value);
        valueLabel->setWordWrap(true);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        form->addRow(nameLabel, valueLabel);
    }

    layout->addLayout(form);
}

void MetadataSidebar::clearStatsSection()
{
    if (!m_statsLayout)
        return;

    while (auto* item = m_statsLayout->takeAt(0)) {
        if (auto* widget = item->widget())
            delete widget;
        else if (auto* childLayout = item->layout()) {
            while (auto* childItem = childLayout->takeAt(0)) {
                if (auto* w = childItem->widget())
                    delete w;
                delete childItem;
            }
            delete childLayout;
        }
        delete item;
    }
}

void MetadataSidebar::setStatsPlaceholder()
{
    clearStatsSection();
    if (!m_statsLayout)
        return;

    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_statsLayout->addWidget(line);

    auto* label = new QLabel("calculating...");
    label->setStyleSheet("color: #999; font-style: italic; padding: 4px 0;");
    m_statsLayout->addWidget(label);
}

void MetadataSidebar::setTileStatistics(const TileStatistics& stats)
{
    clearStatsSection();
    if (!m_statsLayout)
        return;

    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_statsLayout->addWidget(line);

    auto* header = new QLabel("Tile Statistics");
    header->setStyleSheet("font-weight: bold; color: #333; padding: 4px 0;");
    m_statsLayout->addWidget(header);

    QLocale locale;

    auto addStatsForm = [&](const QString& title, const ZoomLevelStats& s) {
        auto* form = new QFormLayout;
        form->setContentsMargins(0, 2, 0, 2);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(4);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

        auto* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold; color: #555;");
        auto* countLabel = new QLabel(locale.toString(s.tileCount) + " tiles");
        countLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(titleLabel, countLabel);

        auto addSizeRow = [&](const QString& name, int64_t value) {
            auto* nameLabel = new QLabel(name);
            nameLabel->setStyleSheet("color: #777; padding-left: 12px;");
            auto* valLabel = new QLabel(FormatUtils::formatTileSize(static_cast<int>(value)));
            valLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            form->addRow(nameLabel, valLabel);
        };

        addSizeRow("p50", s.p50Size);
        addSizeRow("p90", s.p90Size);
        addSizeRow("p99", s.p99Size);

        m_statsLayout->addLayout(form);
    };

    addStatsForm("Total", stats.total);

    for (auto it = stats.perZoom.constBegin(); it != stats.perZoom.constEnd(); ++it)
        addStatsForm(QString("Zoom %1").arg(it.key()), it.value());
}
