#include "widgets/MetadataSidebar.h"

#include <QFormLayout>
#include <QFrame>
#include <QLabel>
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
    }
}

void MetadataSidebar::setMetadata(const TilesetMetadata& metadata)
{
    clear();

    m_contentWidget = new QWidget;
    auto* layout = new QVBoxLayout(m_contentWidget);
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
        if (fields.isEmpty())
            continue;
        addSection(layout, fields, !firstSection);
        firstSection = false;
    }

    layout->addStretch();
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
