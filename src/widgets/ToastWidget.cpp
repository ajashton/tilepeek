#include "widgets/ToastWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

ToastWidget::ToastWidget(Level level, const QString& message, QWidget* parent)
    : QFrame(parent)
    , m_level(level)
{
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);

    QString bgColor, accentColor, icon;
    switch (level) {
    case Level::Error:
        bgColor = "#fde8e8";
        accentColor = "#e53e3e";
        icon = "\xe2\x9d\x8c"; // X mark
        break;
    case Level::Warning:
        bgColor = "#fefce8";
        accentColor = "#d69e2e";
        icon = "\xe2\x9a\xa0\xef\xb8\x8f"; // warning sign
        break;
    case Level::Info:
        bgColor = "#ebf8ff";
        accentColor = "#3182ce";
        icon = "\xe2\x84\xb9\xef\xb8\x8f"; // info
        break;
    }

    setStyleSheet(QString("ToastWidget { background-color: %1; }").arg(bgColor));

    // Single flat layout: accent bar | icon | message | close button
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(8);

    auto* accentBar = new QFrame(this);
    accentBar->setFixedWidth(4);
    accentBar->setStyleSheet(QString("background-color: %1;").arg(accentColor));
    layout->addWidget(accentBar);

    auto* iconLabel = new QLabel(icon, this);
    layout->addWidget(iconLabel);

    m_msgLabel = new QLabel(message, this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setContentsMargins(0, 8, 0, 8);
    m_msgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    layout->addWidget(m_msgLabel, 1);

    auto* closeBtn = new QToolButton(this);
    closeBtn->setText("\xc3\x97"); // multiplication sign as X
    closeBtn->setAutoRaise(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("QToolButton { border: none; font-size: 16px; }");
    layout->addWidget(closeBtn);

    connect(closeBtn, &QToolButton::clicked, this, &ToastWidget::dismissed);
}

int ToastWidget::heightForWidth(int width) const
{
    // Accent bar (4) + spacing (8) + icon (~sizeHint) + spacing (8)
    // + label (expanding) + spacing (8) + close btn (~sizeHint) + right margin (8)
    auto* lay = layout();
    if (!lay)
        return QFrame::heightForWidth(width);

    // Compute the width available for the label: total width minus fixed elements
    int fixedWidth = 4 + 8; // accent bar + first spacing
    // Add icon width
    auto* iconWidget = lay->itemAt(1)->widget();
    fixedWidth += iconWidget->sizeHint().width() + 8; // icon + spacing
    // Add close button width
    auto* closeWidget = lay->itemAt(3)->widget();
    fixedWidth += closeWidget->sizeHint().width() + 8; // close + spacing
    fixedWidth += 8; // right margin

    int labelWidth = width - fixedWidth;
    if (labelWidth < 20)
        labelWidth = 20;

    // Ask the label for its wrapped height (including its own contentsMargins)
    auto labelMargins = m_msgLabel->contentsMargins();
    int textWidth = labelWidth - labelMargins.left() - labelMargins.right();
    int textHeight = m_msgLabel->fontMetrics()
                         .boundingRect(0, 0, textWidth, 0, Qt::TextWordWrap, m_msgLabel->text())
                         .height();
    int labelHeight = textHeight + labelMargins.top() + labelMargins.bottom();

    // The widget height is the max of the label height and the other widgets' heights
    int iconHeight = iconWidget->sizeHint().height();
    int closeHeight = closeWidget->sizeHint().height();
    return std::max({labelHeight, iconHeight, closeHeight});
}
