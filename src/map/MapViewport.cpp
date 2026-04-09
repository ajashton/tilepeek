#include "map/MapViewport.h"
#include "map/WebMercator.h"
#include "util/FormatUtils.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

MapViewport::MapViewport(QWidget* parent)
    : QWidget(parent)
    , m_bgColor(palette().color(QPalette::Window))
{
    setMouseTracking(false);
    setFocusPolicy(Qt::StrongFocus);

    m_zoomSettleTimer.setSingleShot(true);
    m_zoomSettleTimer.setInterval(200);
    connect(&m_zoomSettleTimer, &QTimer::timeout, this, &MapViewport::onZoomSettled);
}

void MapViewport::setTileProvider(TileProvider* provider)
{
    m_provider = provider;
    m_cache.clear();
    m_tileSizeCache.clear();
    update();
}

void MapViewport::setView(double longitude, double latitude, int zoom)
{
    if (!m_provider)
        return;

    m_zoom = std::clamp(zoom, m_provider->minZoom(), m_provider->maxZoom());
    m_scale = 1.0;
    m_centerPixel = QPointF(WebMercator::lonToPixelX(longitude, m_zoom),
                            WebMercator::latToPixelY(latitude, m_zoom));
    clampViewport();
    update();
}

void MapViewport::clear()
{
    m_provider = nullptr;
    m_cache.clear();
    m_crispCache.clear();
    m_crispScale = 0;
    m_tileSizeCache.clear();
    m_zoomSettleTimer.stop();
    m_zoom = 0;
    m_scale = 1.0;
    m_displayTileSize = 256;
    m_centerPixel = QPointF();
    m_bounds.reset();
    m_center.reset();
    update();
}

void MapViewport::setShowTileBoundaries(bool on) { m_showTileBoundaries = on; update(); }
void MapViewport::setShowTileIds(bool on) { m_showTileIds = on; update(); }
void MapViewport::setShowTileSizes(bool on) { m_showTileSizes = on; update(); }
void MapViewport::setShowBounds(bool on) { m_showBounds = on; update(); }
void MapViewport::setShowCenter(bool on) { m_showCenter = on; update(); }

void MapViewport::setBounds(std::optional<ParsedBounds> bounds) { m_bounds = bounds; update(); }
void MapViewport::setCenter(std::optional<ParsedCenter> center) { m_center = center; update(); }
void MapViewport::setBackgroundColor(const QColor& color) { m_bgColor = color; update(); }
void MapViewport::setDisplayTileSize(int size) { m_displayTileSize = size; update(); }

void MapViewport::clearTileCache()
{
    m_cache.clear();
    m_crispCache.clear();
    m_crispScale = 0;
    m_tileSizeCache.clear();
    update();
}

// --- Paint ---

void MapViewport::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), m_bgColor);

    if (!m_provider)
        return;

    double scaledTileSize = m_displayTileSize * m_scale;
    QPointF viewCenter(width() / 2.0, height() / 2.0);
    QRect tiles = visibleTileRange();

    // Pass 1: tile imagery
    bool hasCrisp = (m_crispScale == m_scale && m_crispScale != 0);
    for (int ty = tiles.top(); ty <= tiles.bottom(); ++ty) {
        for (int tx = tiles.left(); tx <= tiles.right(); ++tx) {
            QRectF tileRect = tileScreenRect(tx, ty, viewCenter, scaledTileSize);

            // Prefer crisp (scale-matched) pixmap if available
            TileKey key{m_zoom, tx, ty};
            QPixmap pixmap;
            if (hasCrisp) {
                if (auto crisp = m_crispCache.get(key))
                    pixmap = *crisp;
            }
            if (pixmap.isNull())
                pixmap = fetchTile(m_zoom, tx, ty);

            if (!pixmap.isNull()) {
                painter.drawPixmap(tileRect, pixmap, pixmap.rect());
            } else {
                drawMissingTile(painter, tileRect);
            }
        }
    }

    // Pass 2: tile overlays (on top of all tiles)
    if (m_showTileBoundaries || m_showTileIds || m_showTileSizes)
        drawTileOverlays(painter, tiles, viewCenter, scaledTileSize);

    // Pass 3: geo overlays
    drawBoundsOverlay(painter, viewCenter);
    drawCenterOverlay(painter, viewCenter);
}

// --- Overlay rendering ---

void MapViewport::drawMissingTile(QPainter& painter, const QRectF& tileRect)
{
    painter.fillRect(tileRect, QColor("#d0d0d0"));

    QPen xPen(QColor(200, 60, 60, 160), 2.0);
    painter.setPen(xPen);
    painter.drawLine(tileRect.topLeft(), tileRect.bottomRight());
    painter.drawLine(tileRect.topRight(), tileRect.bottomLeft());
}

void MapViewport::drawTileOverlays(QPainter& painter, const QRect& tiles,
                                    QPointF viewCenter, double scaledTileSize)
{
    QFont monoFont("monospace", 10);
    monoFont.setStyleHint(QFont::Monospace);
    painter.setFont(monoFont);

    for (int ty = tiles.top(); ty <= tiles.bottom(); ++ty) {
        for (int tx = tiles.left(); tx <= tiles.right(); ++tx) {
            QRectF tileRect = tileScreenRect(tx, ty, viewCenter, scaledTileSize);

            if (m_showTileBoundaries) {
                painter.setPen(QPen(QColor(0, 0, 0, 80), 1.0));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(tileRect);
            }

            QStringList lines;

            if (m_showTileIds)
                lines << QString("%1/%2/%3").arg(m_zoom).arg(tx).arg(ty);

            if (m_showTileSizes) {
                auto sizeOpt = fetchTileSize(m_zoom, tx, ty);
                if (sizeOpt && *sizeOpt >= 0)
                    lines << FormatUtils::formatTileSize(*sizeOpt);
                else
                    lines << "missing";
            }

            if (!lines.isEmpty())
                drawTileText(painter, tileRect, lines);
        }
    }
}

void MapViewport::drawTileText(QPainter& painter, const QRectF& tileRect,
                                const QStringList& lines)
{
    constexpr int padding = 4;
    QFontMetrics fm(painter.font());
    int lineHeight = fm.height();

    int textBlockWidth = 0;
    for (const auto& line : lines)
        textBlockWidth = std::max(textBlockWidth, fm.horizontalAdvance(line));

    int textBlockHeight = lines.size() * lineHeight;

    QRectF bgRect(tileRect.left() + padding, tileRect.top() + padding,
                  textBlockWidth + 2 * padding, textBlockHeight + 2 * padding);
    painter.fillRect(bgRect, QColor(255, 255, 255, 180));

    painter.setPen(Qt::black);
    int y = static_cast<int>(tileRect.top()) + padding + fm.ascent() + padding;
    for (const auto& line : lines) {
        painter.drawText(static_cast<int>(tileRect.left()) + 2 * padding, y, line);
        y += lineHeight;
    }
}

void MapViewport::drawBoundsOverlay(QPainter& painter, QPointF viewCenter)
{
    if (!m_showBounds || !m_bounds)
        return;

    QPointF topLeft = geoToScreen(m_bounds->left, m_bounds->top, viewCenter);
    QPointF bottomRight = geoToScreen(m_bounds->right, m_bounds->bottom, viewCenter);
    QRectF boundsRect(topLeft, bottomRight);

    painter.setPen(QPen(QColor(30, 120, 220, 200), 2.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(boundsRect);
}

void MapViewport::drawCenterOverlay(QPainter& painter, QPointF viewCenter)
{
    if (!m_showCenter || !m_center)
        return;

    QPointF screenPos = geoToScreen(m_center->longitude, m_center->latitude, viewCenter);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(220, 40, 40, 220));
    painter.drawEllipse(screenPos, 6.0, 6.0);

    painter.setPen(QPen(Qt::black, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(screenPos, 6.0, 6.0);
}

// --- Coordinate helpers ---

QRectF MapViewport::tileScreenRect(int tx, int ty, QPointF viewCenter, double scaledTileSize) const
{
    QPointF worldPixel(tx * WebMercator::TileSize, ty * WebMercator::TileSize);
    QPointF offset = (worldPixel - m_centerPixel) * m_scale * displayScale();
    return QRectF(viewCenter + offset, QSizeF(scaledTileSize, scaledTileSize));
}

QPointF MapViewport::geoToScreen(double lon, double lat, QPointF viewCenter) const
{
    double wx = WebMercator::lonToPixelX(lon, m_zoom);
    double wy = WebMercator::latToPixelY(lat, m_zoom);
    return viewCenter + (QPointF(wx, wy) - m_centerPixel) * m_scale * displayScale();
}

// --- Input handling ---

void MapViewport::wheelEvent(QWheelEvent* event)
{
    if (!m_provider)
        return;

    double angleDelta = event->angleDelta().y();
    if (angleDelta == 0)
        return;

    // World-space point under cursor (before scale change)
    QPointF viewCenter(width() / 2.0, height() / 2.0);
    QPointF cursorOffset = event->position() - viewCenter;
    QPointF worldUnderCursor = m_centerPixel + cursorOffset / (m_scale * displayScale());

    double scaleStep = 1.0 + (angleDelta / 120.0) * 0.1;
    double newScale = m_scale * scaleStep;

    if (newScale >= 2.0 && m_zoom < m_provider->maxZoom()) {
        worldUnderCursor *= 2.0;
        transitionZoom(m_zoom + 1);
    } else if (newScale < 1.0 && m_zoom > m_provider->minZoom()) {
        worldUnderCursor /= 2.0;
        transitionZoom(m_zoom - 1);
    } else if (newScale >= 2.0) {
        m_scale = 1.99;
    } else if (newScale < 1.0) {
        m_scale = 1.0;
    } else {
        m_scale = newScale;
    }

    // Adjust center so the world point stays under the cursor
    m_centerPixel = worldUnderCursor - cursorOffset / (m_scale * displayScale());
    clampViewport();

    // Invalidate crisp cache if scale changed
    if (m_crispScale != m_scale) {
        m_crispCache.clear();
        m_crispScale = 0;
    }

    // Restart debounce timer for crisp re-render
    m_zoomSettleTimer.start();

    update();
    event->accept();
}

void MapViewport::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
}

void MapViewport::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_centerPixel -= QPointF(delta) / (m_scale * displayScale());
        m_lastMousePos = event->pos();
        clampViewport();
        update();
        event->accept();
    }
}

void MapViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    }
}

// --- Internal state ---

void MapViewport::onZoomSettled()
{
    if (!m_provider)
        return;

    int crispSize = static_cast<int>(std::round(m_displayTileSize * m_scale));
    if (crispSize == m_displayTileSize)
        return; // at 1.0 scale, base pixmaps are already crisp

    QRect tiles = visibleTileRange();
    for (int ty = tiles.top(); ty <= tiles.bottom(); ++ty) {
        for (int tx = tiles.left(); tx <= tiles.right(); ++tx) {
            TileKey key{m_zoom, tx, ty};
            if (m_crispCache.get(key))
                continue; // already have this one

            if (auto pixmap = m_provider->tileAtSize(m_zoom, tx, ty, crispSize))
                m_crispCache.insert(key, *pixmap);
        }
    }

    m_crispScale = m_scale;
    update();
}

void MapViewport::clampViewport()
{
    double maxPixel = static_cast<double>(WebMercator::mapSizePixels(m_zoom));
    m_centerPixel.setX(std::clamp(m_centerPixel.x(), 0.0, maxPixel));
    m_centerPixel.setY(std::clamp(m_centerPixel.y(), 0.0, maxPixel));
}

void MapViewport::transitionZoom(int newZoom)
{
    if (newZoom > m_zoom) {
        m_centerPixel *= 2.0;
        m_scale /= 2.0;
    } else {
        m_centerPixel /= 2.0;
        m_scale *= 2.0;
    }
    m_zoom = newZoom;
    m_cache.evictOtherZooms(m_zoom);
    m_crispCache.clear();
    m_crispScale = 0;

    // Evict tile sizes for other zoom levels
    std::erase_if(m_tileSizeCache, [&](const auto& entry) {
        return entry.first.zoom != m_zoom;
    });
}

QRect MapViewport::visibleTileRange() const
{
    double ds = displayScale();
    double halfViewW = (width() / 2.0) / (m_scale * ds);
    double halfViewH = (height() / 2.0) / (m_scale * ds);

    double minPx = m_centerPixel.x() - halfViewW;
    double maxPx = m_centerPixel.x() + halfViewW;
    double minPy = m_centerPixel.y() - halfViewH;
    double maxPy = m_centerPixel.y() + halfViewH;

    int maxTile = (1 << m_zoom) - 1;

    int minTileX = std::clamp(static_cast<int>(std::floor(minPx / WebMercator::TileSize)), 0, maxTile);
    int maxTileX = std::clamp(static_cast<int>(std::floor(maxPx / WebMercator::TileSize)), 0, maxTile);
    int minTileY = std::clamp(static_cast<int>(std::floor(minPy / WebMercator::TileSize)), 0, maxTile);
    int maxTileY = std::clamp(static_cast<int>(std::floor(maxPy / WebMercator::TileSize)), 0, maxTile);

    return QRect(QPoint(minTileX, minTileY), QPoint(maxTileX, maxTileY));
}

QPixmap MapViewport::fetchTile(int zoom, int x, int y)
{
    TileKey key{zoom, x, y};
    if (auto cached = m_cache.get(key))
        return *cached;
    if (m_provider) {
        if (auto pixmap = m_provider->tileAt(zoom, x, y)) {
            m_cache.insert(key, *pixmap);
            return *pixmap;
        }
    }
    return {};
}

std::optional<int> MapViewport::fetchTileSize(int zoom, int x, int y)
{
    TileKey key{zoom, x, y};
    auto it = m_tileSizeCache.find(key);
    if (it != m_tileSizeCache.end())
        return it->second;

    std::optional<int> size;
    if (m_provider)
        size = m_provider->tileSizeAt(zoom, x, y);
    m_tileSizeCache[key] = size;
    return size;
}
