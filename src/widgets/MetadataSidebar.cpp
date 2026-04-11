#include "widgets/MetadataSidebar.h"
#include "mbtiles/VectorMetadataParser.h"
#include "model/TileStatistics.h"
#include "util/FormatUtils.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextLayout>
#include <QTabWidget>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

namespace {

void setSubduedTextColor(QWidget* widget)
{
    auto pal = widget->palette();
    auto color = pal.color(QPalette::WindowText);
    color.setAlphaF(0.55);
    pal.setColor(QPalette::WindowText, color);
    widget->setPalette(pal);
}

class TileSizeBarWidget : public QWidget {
public:
    TileSizeBarWidget(int zoom, const ZoomLevelStats& stats, int64_t globalMax, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_zoom(zoom)
        , m_stats(stats)
        , m_globalMax(globalMax)
    {
        setFixedHeight(16);
        setMouseTracking(true);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int labelW = 20;
        int barX = labelW;
        int barW = width() - labelW;
        int barH = height() - 2;
        int barY = 1;

        // Zoom label
        p.setPen(palette().color(QPalette::Text));
        QFont f = font();
        f.setPointSize(f.pointSize() - 1);
        p.setFont(f);
        p.drawText(QRect(0, 0, labelW - 4, height()), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(m_zoom));

        if (m_globalMax <= 0 || barW <= 0)
            return;

        auto xFor = [&](int64_t size) {
            return static_cast<int>(static_cast<double>(size) / m_globalMax * barW);
        };

        // Draw segments from widest (max) to narrowest (p50) so they stack
        struct Segment {
            int64_t size;
            QColor color;
        };

        // Use palette-aware greys
        auto base = palette().color(QPalette::Text);
        auto withAlpha = [&](int alpha) {
            QColor c = base;
            c.setAlpha(alpha);
            return c;
        };

        Segment segments[] = {
            {m_stats.maxSize, withAlpha(50)},
            {m_stats.p99Size, withAlpha(80)},
            {m_stats.p90Size, withAlpha(120)},
            {m_stats.p50Size, withAlpha(180)},
        };

        for (const auto& seg : segments) {
            int w = xFor(seg.size);
            if (w > 0) {
                p.fillRect(barX, barY, w, barH, seg.color);
            }
        }
    }

    bool event(QEvent* e) override
    {
        if (e->type() == QEvent::ToolTip) {
            auto* he = static_cast<QHelpEvent*>(e);
            QString tip = QString("Zoom %1 — %2 tiles\n"
                                  "p50: %3 | p90: %4 | p99: %5 | max: %6")
                              .arg(m_zoom)
                              .arg(QLocale().toString(m_stats.tileCount))
                              .arg(FormatUtils::formatTileSize(static_cast<int>(m_stats.p50Size)))
                              .arg(FormatUtils::formatTileSize(static_cast<int>(m_stats.p90Size)))
                              .arg(FormatUtils::formatTileSize(static_cast<int>(m_stats.p99Size)))
                              .arg(FormatUtils::formatTileSize(static_cast<int>(m_stats.maxSize)));
            QToolTip::showText(he->globalPos(), tip, this);
            return true;
        }
        return QWidget::event(e);
    }

private:
    int m_zoom;
    ZoomLevelStats m_stats;
    int64_t m_globalMax;
};

// QLabel's word wrap uses Qt::TextWordWrap which never breaks mid-word.
// This subclass uses WrapAtWordBoundaryOrAnywhere so that long unbroken
// words (URLs, hashes, etc.) break mid-word rather than overflowing.
class WrappingLabel : public QLabel {
public:
    using QLabel::QLabel;

    int heightForWidth(int w) const override
    {
        auto m = contentsMargins();
        int textW = w - m.left() - m.right();
        if (textW <= 0)
            return QLabel::heightForWidth(w);

        QTextLayout layout(text(), font());
        QTextOption opt;
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(opt);

        layout.beginLayout();
        qreal height = 0;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(textW);
            height += line.leading() + line.height();
        }
        layout.endLayout();

        return static_cast<int>(std::ceil(height)) + m.top() + m.bottom();
    }

    QSize minimumSizeHint() const override
    {
        return {0, QLabel::minimumSizeHint().height()};
    }

    QSize sizeHint() const override
    {
        QSize s = QLabel::sizeHint();
        if (width() > 0)
            s.setHeight(heightForWidth(width()));
        return s;
    }

    bool hasHeightForWidth() const override { return true; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(palette().color(foregroundRole()));
        p.setFont(font());

        QTextLayout layout(text(), font());
        QTextOption opt;
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(opt);

        auto m = contentsMargins();
        qreal textW = width() - m.left() - m.right();

        layout.beginLayout();
        qreal y = m.top();
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(textW);
            line.setPosition(QPointF(m.left(), y));
            y += line.leading() + line.height();
        }
        layout.endLayout();
        layout.draw(&p, QPointF());
    }
};

} // namespace

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
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    m_rawJson = QJsonObject();
    m_inspectTabIndex = -1;
    m_selectedFeatureIndex = -1;
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
                                         const VectorMetadata& vectorMeta,
                                         const QList<QColor>& layerColors)
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
    metaScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    metaScroll->setWidget(metaWidget);
    m_contentWidget = metaWidget;
    m_tabWidget->addTab(metaScroll, "Metadata");

    // Layers tab
    auto* layersWidget = buildLayersWidget(vectorMeta.vectorLayers, layerColors);
    auto* layersScroll = new QScrollArea;
    layersScroll->setWidgetResizable(true);
    layersScroll->setFrameShape(QFrame::NoFrame);
    layersScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layersScroll->setWidget(layersWidget);
    m_tabWidget->addTab(layersScroll, "Layers");

    // Tilestats tab (only if present)
    if (vectorMeta.hasTilestats) {
        auto* statsWidget = buildTilestatsWidget(vectorMeta.tilestats);
        auto* statsScroll = new QScrollArea;
        statsScroll->setWidgetResizable(true);
        statsScroll->setFrameShape(QFrame::NoFrame);
        statsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        statsScroll->setWidget(statsWidget);
        m_tabWidget->addTab(statsScroll, "Stats");
    }

    m_rawJson = vectorMeta.rawJson;
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

    if (skipJson) {
        auto* line = new QFrame;
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line);

        auto* jsonBtn = new QPushButton("View metadata JSON\u2026");
        connect(jsonBtn, &QPushButton::clicked, this, &MetadataSidebar::showJsonWindow);
        layout->addWidget(jsonBtn);
    }

    return widget;
}

QWidget* MetadataSidebar::buildLayersWidget(const QList<VectorLayerInfo>& layers,
                                             const QList<QColor>& layerColors)
{
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);

    m_layerCheckboxes.clear();

    for (int i = 0; i < layers.size(); ++i) {
        const auto& layer = layers[i];
        QColor color = (i < layerColors.size()) ? layerColors[i] : QColor("#646464");

        if (i > 0) {
            auto* line = new QFrame;
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            layout->addWidget(line);
        }

        // Header row
        auto* headerLayout = new QHBoxLayout;
        headerLayout->setContentsMargins(0, 2, 4, 2);
        headerLayout->setSpacing(2);

        auto* arrow = new QToolButton;
        arrow->setArrowType(Qt::RightArrow);
        arrow->setAutoRaise(true);
        arrow->setCheckable(true);
        arrow->setFixedSize(20, 20);

        auto* swatch = new QFrame;
        swatch->setFixedSize(16, 16);
        swatch->setStyleSheet(QString("background-color: %1;").arg(color.name()));

        auto* checkbox = new QCheckBox(layer.id);
        checkbox->setChecked(true);
        checkbox->setStyleSheet("font-weight: bold;");
        connect(checkbox, &QCheckBox::toggled, this, &MetadataSidebar::onLayerCheckboxToggled);
        m_layerCheckboxes[layer.id] = checkbox;

        auto* zoomLabel = new QLabel(QString("z%1\xe2\x80\x93%2").arg(layer.minzoom).arg(layer.maxzoom));
        setSubduedTextColor(zoomLabel);

        headerLayout->addWidget(arrow);
        headerLayout->addWidget(swatch);
        headerLayout->addWidget(checkbox, 1);
        headerLayout->addWidget(zoomLabel);

        layout->addLayout(headerLayout);

        // Detail widget (initially hidden)
        auto* detail = new QWidget;
        auto* detailLayout = new QVBoxLayout(detail);
        detailLayout->setContentsMargins(28, 0, 8, 4);
        detailLayout->setSpacing(4);

        if (layer.description.isEmpty() && layer.fields.isEmpty()) {
            auto* emptyLabel = new QLabel("No description or fields");
            emptyLabel->setStyleSheet("font-style: italic;");
            setSubduedTextColor(emptyLabel);
            detailLayout->addWidget(emptyLabel);
        }

        if (!layer.description.isEmpty()) {
            auto* descLabel = new QLabel(layer.description);
            descLabel->setWordWrap(true);
            descLabel->setStyleSheet("font-style: italic;");
            setSubduedTextColor(descLabel);
            detailLayout->addWidget(descLabel);
        }

        if (!layer.fields.isEmpty()) {
            auto* form = new QFormLayout;
            form->setContentsMargins(0, 2, 0, 0);
            form->setHorizontalSpacing(12);
            form->setVerticalSpacing(2);
            form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

            for (auto it = layer.fields.constBegin(); it != layer.fields.constEnd(); ++it) {
                auto* nameLabel = new QLabel(it.key());
                QFont mono("monospace");
                mono.setStyleHint(QFont::Monospace);
                nameLabel->setFont(mono);

                auto* typeLabel = new QLabel(it.value());
                typeLabel->setWordWrap(true);
                setSubduedTextColor(typeLabel);

                form->addRow(nameLabel, typeLabel);
            }

            detailLayout->addLayout(form);
        }

        detail->setVisible(false);
        layout->addWidget(detail);

        // Connect arrow toggle
        connect(arrow, &QToolButton::toggled, this, [arrow, detail](bool expanded) {
            arrow->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            detail->setVisible(expanded);
        });
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

void MetadataSidebar::showJsonWindow()
{
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle("Metadata JSON");
    dialog->resize(600, 500);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(dialog);

    QJsonDocument doc(m_rawJson);
    auto* text = new QPlainTextEdit(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    text->setReadOnly(true);
    QFont mono("monospace", 9);
    mono.setStyleHint(QFont::Monospace);
    text->setFont(mono);
    layout->addWidget(text);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    dialog->show();
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

        auto* valueLabel = new WrappingLabel(field.value);
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

    auto* header = new QLabel("Statistics");
    header->setStyleSheet("font-weight: bold; padding: 4px 0;");
    m_statsLayout->addWidget(header);

    auto* subhead = new QLabel("Tile size by zoom level:");
    subhead->setStyleSheet("padding: 0 0 2px 0;");
    m_statsLayout->addWidget(subhead);

    // Find the global maximum tile size across all zoom levels
    int64_t globalMax = 0;
    for (auto it = stats.perZoom.constBegin(); it != stats.perZoom.constEnd(); ++it)
        globalMax = std::max(globalMax, it.value().maxSize);

    // One bar per zoom level
    for (auto it = stats.perZoom.constBegin(); it != stats.perZoom.constEnd(); ++it) {
        auto* bar = new TileSizeBarWidget(it.key(), it.value(), globalMax);
        m_statsLayout->addWidget(bar);
    }

    // Max size label aligned right
    if (globalMax > 0) {
        auto* maxLabel = new QLabel("max: " + FormatUtils::formatTileSize(static_cast<int>(globalMax)));
        maxLabel->setAlignment(Qt::AlignRight);
        maxLabel->setStyleSheet("padding: 2px 0;");
        m_statsLayout->addWidget(maxLabel);
    }
}

void MetadataSidebar::setInspectResults(const QList<mvt::HitTestResult>& results)
{
    if (!m_tabWidget || results.isEmpty())
        return;

    clearInspectResults();

    auto* inspectWidget = buildInspectWidget(results);
    auto* inspectScroll = new QScrollArea;
    inspectScroll->setWidgetResizable(true);
    inspectScroll->setFrameShape(QFrame::NoFrame);
    inspectScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inspectScroll->setWidget(inspectWidget);

    m_inspectTabIndex = m_tabWidget->addTab(inspectScroll, "Inspect");
    m_tabWidget->setCurrentIndex(m_inspectTabIndex);
}

void MetadataSidebar::clearInspectResults()
{
    if (!m_tabWidget || m_inspectTabIndex < 0)
        return;

    m_tabWidget->removeTab(m_inspectTabIndex);
    m_inspectTabIndex = -1;
    m_selectedFeatureIndex = -1;
}

QWidget* MetadataSidebar::buildInspectWidget(const QList<mvt::HitTestResult>& results)
{
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);

    // Keep track of header widgets for selection highlighting
    QList<QWidget*> headerWidgets;

    for (int i = 0; i < results.size(); ++i) {
        const auto& result = results[i];

        if (i > 0) {
            auto* line = new QFrame;
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            layout->addWidget(line);
        }

        // Header row container (clickable)
        auto* headerContainer = new QWidget;
        auto* headerLayout = new QHBoxLayout(headerContainer);
        headerLayout->setContentsMargins(0, 2, 4, 2);
        headerLayout->setSpacing(2);

        auto* arrow = new QToolButton;
        arrow->setArrowType(i == 0 ? Qt::DownArrow : Qt::RightArrow);
        arrow->setAutoRaise(true);
        arrow->setCheckable(true);
        arrow->setChecked(i == 0);
        arrow->setFixedSize(20, 20);

        auto* swatch = new QFrame;
        swatch->setFixedSize(16, 16);
        swatch->setStyleSheet(
            QString("background-color: %1;").arg(result.layerColor.name()));

        auto* nameLabel = new QLabel(result.layerName);
        nameLabel->setStyleSheet("font-weight: bold;");

        // Geometry type label
        QString geomText;
        switch (result.geomType) {
        case mvt::GeomType::Point: geomText = "Point"; break;
        case mvt::GeomType::LineString: geomText = "Line"; break;
        case mvt::GeomType::Polygon: geomText = "Polygon"; break;
        default: geomText = "Unknown"; break;
        }
        auto* geomLabel = new QLabel(geomText);
        setSubduedTextColor(geomLabel);

        // Feature ID label
        QString idText = result.featureId
                             ? QString::number(*result.featureId)
                             : "(no id)";
        auto* idLabel = new QLabel(idText);
        setSubduedTextColor(idLabel);

        headerLayout->addWidget(arrow);
        headerLayout->addWidget(swatch);
        headerLayout->addWidget(nameLabel, 1);
        headerLayout->addWidget(geomLabel);
        headerLayout->addWidget(idLabel);

        layout->addWidget(headerContainer);
        headerWidgets.append(headerContainer);

        // Detail widget
        auto* detail = new QWidget;
        auto* detailLayout = new QVBoxLayout(detail);
        detailLayout->setContentsMargins(28, 0, 8, 4);
        detailLayout->setSpacing(4);

        if (result.properties.isEmpty()) {
            auto* emptyLabel = new QLabel("No properties");
            emptyLabel->setStyleSheet("font-style: italic;");
            setSubduedTextColor(emptyLabel);
            detailLayout->addWidget(emptyLabel);
        } else {
            auto* form = new QFormLayout;
            form->setContentsMargins(0, 2, 0, 0);
            form->setHorizontalSpacing(12);
            form->setVerticalSpacing(2);
            form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

            for (const auto& [key, value] : result.properties) {
                auto* keyLabel = new QLabel(key);
                QFont mono("monospace");
                mono.setStyleHint(QFont::Monospace);
                keyLabel->setFont(mono);

                auto* valueLabel = new WrappingLabel(value);
                valueLabel->setWordWrap(true);
                valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

                form->addRow(keyLabel, valueLabel);
            }
            detailLayout->addLayout(form);
        }

        detail->setVisible(i == 0);
        layout->addWidget(detail);

        // Connect arrow toggle
        connect(arrow, &QToolButton::toggled, this, [arrow, detail](bool expanded) {
            arrow->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            detail->setVisible(expanded);
        });

        // Click on header to isolate highlight
        headerContainer->setCursor(Qt::PointingHandCursor);
        headerContainer->installEventFilter(widget);
    }

    // Install event filter on the container widget to handle header clicks
    // We use a lambda-based approach with a custom event filter
    auto* filterObj = new QObject(widget);
    connect(filterObj, &QObject::destroyed, this, []() {}); // prevent dangling

    // Store context for the event filter
    struct FilterContext {
        MetadataSidebar* sidebar;
        QList<QWidget*> headers;
    };
    auto* ctx = new FilterContext{this, headerWidgets};

    // Use the widget itself as the event filter
    class HeaderClickFilter : public QObject {
    public:
        HeaderClickFilter(FilterContext* ctx, QObject* parent)
            : QObject(parent), m_ctx(ctx) {}
        ~HeaderClickFilter() override { delete m_ctx; }

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override
        {
            if (event->type() != QEvent::MouseButtonPress)
                return false;

            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() != Qt::LeftButton)
                return false;

            auto* headerWidget = qobject_cast<QWidget*>(obj);
            if (!headerWidget)
                return false;

            int clickedIndex = m_ctx->headers.indexOf(headerWidget);
            if (clickedIndex < 0)
                return false;

            // Toggle selection
            int newIndex = (m_ctx->sidebar->m_selectedFeatureIndex == clickedIndex)
                               ? -1
                               : clickedIndex;
            m_ctx->sidebar->m_selectedFeatureIndex = newIndex;

            // Update visual highlighting
            auto baseColor = m_ctx->headers[0]->palette().color(QPalette::Window);
            auto highlightColor = m_ctx->headers[0]->palette().color(QPalette::Highlight);
            highlightColor.setAlpha(40);

            for (int i = 0; i < m_ctx->headers.size(); ++i) {
                auto* w = m_ctx->headers[i];
                if (i == newIndex) {
                    w->setAutoFillBackground(true);
                    auto pal = w->palette();
                    pal.setColor(QPalette::Window, highlightColor);
                    w->setPalette(pal);
                } else {
                    w->setAutoFillBackground(false);
                    w->setPalette(w->parentWidget()->palette());
                }
            }

            emit m_ctx->sidebar->featureIsolated(newIndex);
            return false; // don't consume -- let arrow clicks still work
        }

    private:
        FilterContext* m_ctx;
    };

    auto* filter = new HeaderClickFilter(ctx, widget);
    for (auto* header : headerWidgets)
        header->installEventFilter(filter);

    layout->addStretch();
    return widget;
}
