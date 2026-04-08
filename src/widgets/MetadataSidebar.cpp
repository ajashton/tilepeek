#include "widgets/MetadataSidebar.h"
#include "model/TileStatistics.h"
#include "util/FormatUtils.h"

#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QVBoxLayout>

MetadataSidebar::MetadataSidebar(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* header = new QLabel("Metadata", this);
    header->setStyleSheet("font-weight: bold; font-size: 14px; padding: 8px;");
    outerLayout->addWidget(header);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    outerLayout->addWidget(m_scrollArea);

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
}

void MetadataSidebar::setMetadata(const TilesetMetadata& metadata)
{
    clear();

    m_contentWidget = new QWidget;
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(8, 4, 8, 8);

    static constexpr FieldCategory categories[] = {
        FieldCategory::Required,
        FieldCategory::Recommended,
        FieldCategory::Standard,
        FieldCategory::NonStandard,
    };

    bool firstSection = true;
    for (auto category : categories) {
        auto fields = metadata.fieldsByCategory(category);
        if (fields.isEmpty())
            continue;
        addSection(m_contentLayout, fields, !firstSection);
        firstSection = false;
    }

    // Reserve space for stats section
    m_statsLayout = new QVBoxLayout;
    m_contentLayout->addLayout(m_statsLayout);

    m_contentLayout->addStretch();
    m_scrollArea->setWidget(m_contentWidget);
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
