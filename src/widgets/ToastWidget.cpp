#include "widgets/ToastWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

ToastWidget::ToastWidget(Level level, const QString& message, QWidget* parent)
    : QFrame(parent)
    , m_level(level)
{
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);

    QString bgColor, borderColor, icon;
    switch (level) {
    case Level::Error:
        bgColor = "#fde8e8";
        borderColor = "#e53e3e";
        icon = "\xe2\x9d\x8c"; // X mark
        break;
    case Level::Warning:
        bgColor = "#fefce8";
        borderColor = "#d69e2e";
        icon = "\xe2\x9a\xa0\xef\xb8\x8f"; // warning sign
        break;
    case Level::Info:
        bgColor = "#ebf8ff";
        borderColor = "#3182ce";
        icon = "\xe2\x84\xb9\xef\xb8\x8f"; // info
        break;
    }

    setStyleSheet(QString(
        "ToastWidget {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-left: 4px solid %2;"
        "  border-radius: 4px;"
        "  padding: 8px;"
        "}")
                      .arg(bgColor, borderColor));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    auto* iconLabel = new QLabel(icon, this);
    layout->addWidget(iconLabel);

    auto* msgLabel = new QLabel(message, this);
    msgLabel->setWordWrap(true);
    msgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(msgLabel, 1);

    auto* closeBtn = new QToolButton(this);
    closeBtn->setText("\xc3\x97"); // multiplication sign as X
    closeBtn->setAutoRaise(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("QToolButton { border: none; font-size: 16px; }");
    layout->addWidget(closeBtn);

    connect(closeBtn, &QToolButton::clicked, this, &ToastWidget::dismissed);

    setFixedWidth(360);
}
