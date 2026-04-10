#include "widgets/EmptyStateWidget.h"

#include <QImage>
#include <QPainter>

static constexpr int PatternTileSize = 512;
static constexpr int TextPointSize = 14;
static constexpr int BoxPadding = 32;
static constexpr int BoxRadius = 12;

EmptyStateWidget::EmptyStateWidget(QWidget* parent)
    : QWidget(parent)
{
    QImage img(QStringLiteral(":/graphics/topography.svg"));
    if (!img.isNull())
        m_patternTile = QPixmap::fromImage(
            img.scaled(PatternTileSize, PatternTileSize, Qt::IgnoreAspectRatio,
                       Qt::SmoothTransformation));
}

void EmptyStateWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);

    // 1. Fill with window background
    QColor bgColor = palette().color(QPalette::Window);
    painter.fillRect(rect(), bgColor);

    // 2. Tile the SVG pattern
    if (!m_patternTile.isNull())
        painter.drawTiledPixmap(rect(), m_patternTile);

    // 3. Measure the text
    QString text = QStringLiteral("Drag & drop or open\na MBTiles or PMTiles file");
    QFont textFont = font();
    textFont.setPointSize(TextPointSize);
    textFont.setWeight(QFont::Normal);
    painter.setFont(textFont);

    QFontMetrics fm(textFont);
    QRect textBound = fm.boundingRect(rect(), Qt::AlignCenter, text);

    // 4. Draw rounded rectangle behind text
    QRect boxRect = textBound.adjusted(-BoxPadding, -BoxPadding, BoxPadding, BoxPadding);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawRoundedRect(boxRect, BoxRadius, BoxRadius);

    // 5. Draw the text
    QColor textColor = palette().color(QPalette::WindowText);
    textColor.setAlphaF(0.5f);
    painter.setPen(textColor);
    painter.drawText(rect(), Qt::AlignCenter, text);
}
