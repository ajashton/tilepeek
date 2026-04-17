#include "map/MapViewport.h"
#include "map/WebMercator.h"
#include "util/FormatUtils.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
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

    m_dpr = devicePixelRatioF();

    m_zoomSettleTimer.setSingleShot(true);
    m_zoomSettleTimer.setInterval(200);
    connect(&m_zoomSettleTimer, &QTimer::timeout, this, &MapViewport::onZoomSettled);
}

void MapViewport::setTileProvider(TileProvider* provider)
{
    m_provider = provider;
    m_cache.clear();
    m_tileSizeCache.clear();
    if (m_provider)
        m_provider->setDevicePixelRatio(m_dpr);
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
    emit zoomChanged(m_zoom);
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
    m_isVectorProvider = false;
    m_tileFocusActive = false;
    m_tileFocusSelecting = false;
    m_focusedTilePixmap = QPixmap();
    m_focusBufferRatio = 0.0;
    m_inspectHighlights.clear();
    m_inspectTileSize = 0;
    m_isolatedHighlight = -1;
    update();
}

void MapViewport::zoomIn()
{
    if (!m_provider)
        return;
    if (m_tileFocusActive) {
        m_scale = std::min(m_scale * 2.0, 8.0);
    } else {
        if (m_zoom >= m_provider->maxZoom())
            return;
        transitionZoom(m_zoom + 1);
        m_scale = 1.0;
        emit zoomChanged(m_zoom);
    }
    m_crispCache.clear();
    m_crispScale = 0;
    m_zoomSettleTimer.start();
    update();
}

void MapViewport::zoomOut()
{
    if (!m_provider)
        return;
    if (m_tileFocusActive) {
        m_scale = std::max(m_scale / 2.0, 0.5);
    } else {
        if (m_zoom <= m_provider->minZoom())
            return;
        transitionZoom(m_zoom - 1);
        m_scale = 1.0;
        emit zoomChanged(m_zoom);
    }
    m_crispCache.clear();
    m_crispScale = 0;
    m_zoomSettleTimer.start();
    update();
}

void MapViewport::setZoom(int zoom)
{
    if (!m_provider || m_tileFocusActive)
        return;
    zoom = std::clamp(zoom, m_provider->minZoom(), m_provider->maxZoom());
    if (zoom == m_zoom)
        return;

    // Scale center pixel coordinates for multi-level zoom jump
    int delta = zoom - m_zoom;
    double factor = std::pow(2.0, delta);
    m_centerPixel *= factor;

    m_zoom = zoom;
    m_scale = 1.0;
    m_cache.evictOtherZooms(m_zoom);
    m_crispCache.clear();
    m_crispScale = 0;
    std::erase_if(m_tileSizeCache, [&](const auto& entry) {
        return entry.first.zoom != m_zoom;
    });
    clampViewport();
    m_zoomSettleTimer.start();

    // Clear inspect highlights on integer zoom change
    if (!m_inspectHighlights.isEmpty()) {
        m_inspectHighlights.clear();
        m_inspectTileSize = 0;
        m_isolatedHighlight = -1;
        emit inspectCleared();
    }

    emit zoomChanged(m_zoom);
    update();
}

void MapViewport::zoomToBounds(double left, double bottom, double right, double top)
{
    if (!m_provider || width() <= 0 || height() <= 0)
        return;

    // Find the highest zoom level where the bounds fit in the viewport
    double ds = displayScale();
    int bestZoom = m_provider->minZoom();

    for (int z = m_provider->maxZoom(); z >= m_provider->minZoom(); --z) {
        double pxLeft = WebMercator::lonToPixelX(left, z);
        double pxRight = WebMercator::lonToPixelX(right, z);
        double pxTop = WebMercator::latToPixelY(top, z);
        double pxBottom = WebMercator::latToPixelY(bottom, z);

        double boundsW = (pxRight - pxLeft) * ds;
        double boundsH = (pxBottom - pxTop) * ds;

        if (boundsW <= width() && boundsH <= height()) {
            bestZoom = z;
            break;
        }
    }

    // Center on the bounds
    double centerLon = (left + right) / 2.0;
    double centerLat = (top + bottom) / 2.0;

    setView(centerLon, centerLat, bestZoom);
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

void MapViewport::setTileFocusSelecting(bool on)
{
    m_tileFocusSelecting = on;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

TileKey MapViewport::tileAtScreenPos(const QPoint& pos) const
{
    QPointF viewCenter(width() / 2.0, height() / 2.0);
    QPointF worldPos = m_centerPixel + (QPointF(pos) - viewCenter) / (m_scale * displayScale());
    int tx = std::clamp(static_cast<int>(std::floor(worldPos.x() / WebMercator::TileSize)),
                        0, (1 << m_zoom) - 1);
    int ty = std::clamp(static_cast<int>(std::floor(worldPos.y() / WebMercator::TileSize)),
                        0, (1 << m_zoom) - 1);
    return {m_zoom, tx, ty};
}

void MapViewport::focusTileAt(const QPoint& screenPos)
{
    if (!m_provider)
        return;

    auto key = tileAtScreenPos(screenPos);
    int renderSize = static_cast<int>(m_displayTileSize * m_scale);
    auto result = m_provider->tileUnclipped(key.zoom, key.x, key.y, renderSize);
    if (!result)
        return;

    m_tileFocusActive = true;
    m_focusedTile = key;
    m_focusedTilePixmap = result->pixmap;
    m_focusBufferRatio = result->bufferRatio;
    m_tileFocusSelecting = false;
    setCursor(Qt::ArrowCursor);
    emit tileFocusChanged(true);
    update();
}

void MapViewport::exitTileFocus()
{
    if (!m_tileFocusActive)
        return;
    m_tileFocusActive = false;
    m_focusedTilePixmap = QPixmap();
    m_tileFocusSelecting = false;
    setCursor(Qt::ArrowCursor);

    // Clear inspect highlights when exiting focus mode
    if (!m_inspectHighlights.isEmpty()) {
        m_inspectHighlights.clear();
        m_inspectTileSize = 0;
        m_isolatedHighlight = -1;
        emit inspectCleared();
    }

    // Clamp scale back to normal range
    if (m_scale >= 2.0 || m_scale < 1.0) {
        m_scale = std::clamp(m_scale, 1.0, 1.99);
        m_crispCache.clear();
        m_crispScale = 0;
        m_zoomSettleTimer.start();
    }

    emit tileFocusChanged(false);
    update();
}

// --- Paint ---

bool MapViewport::event(QEvent* e)
{
    if (e->type() == QEvent::DevicePixelRatioChange) {
        qreal newDpr = devicePixelRatioF();
        if (newDpr != m_dpr) {
            m_dpr = newDpr;
            if (m_provider)
                m_provider->setDevicePixelRatio(m_dpr);
            m_cache.clear();
            m_crispCache.clear();
            m_crispScale = 0;
            m_focusedTilePixmap = QPixmap();
            if (m_tileFocusActive)
                m_zoomSettleTimer.start();
            update();
        }
    }
    return QWidget::event(e);
}

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
    if (m_tileFocusActive)
        painter.setOpacity(0.3);

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
    if (!m_tileFocusActive && (m_showTileBoundaries || m_showTileIds || m_showTileSizes))
        drawTileOverlays(painter, tiles, viewCenter, scaledTileSize);

    if (m_tileFocusActive)
        painter.setOpacity(1.0);

    // Pass 3: focused tile (drawn at full opacity on top of subdued tiles)
    if (m_tileFocusActive && !m_focusedTilePixmap.isNull()) {
        QRectF tileRect = tileScreenRect(m_focusedTile.x, m_focusedTile.y,
                                          viewCenter, scaledTileSize);
        // Expand rect to include buffer area
        double bufferScreenSize = scaledTileSize * m_focusBufferRatio;
        QRectF expandedRect = tileRect.adjusted(-bufferScreenSize, -bufferScreenSize,
                                                 bufferScreenSize, bufferScreenSize);
        painter.drawPixmap(expandedRect, m_focusedTilePixmap, m_focusedTilePixmap.rect());

        // Draw tile extent boundary
        painter.setPen(QPen(QColor(255, 255, 255, 128), 1.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(tileRect);
    }

    // Pass 4: geo overlays
    if (!m_tileFocusActive) {
        drawBoundsOverlay(painter, viewCenter);
        drawCenterOverlay(painter, viewCenter);
    }

    // Pass 5: feature inspection highlights
    if (!m_inspectHighlights.isEmpty())
        drawInspectHighlights(painter, viewCenter, scaledTileSize);
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
                painter.setPen(QPen(QColor(255, 255, 255, 64), 1.0));
                // Draw bottom and right edges of each tile; draw top/left
                // edges only for the first row/column to avoid double lines
                // along interior seams.
                painter.drawLine(tileRect.bottomLeft(), tileRect.bottomRight());
                painter.drawLine(tileRect.topRight(), tileRect.bottomRight());
                if (ty == tiles.top())
                    painter.drawLine(tileRect.topLeft(), tileRect.topRight());
                if (tx == tiles.left())
                    painter.drawLine(tileRect.topLeft(), tileRect.bottomLeft());
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
    constexpr int padding = 2;
    QFontMetrics fm(painter.font());
    int lineHeight = fm.height();

    int textBlockWidth = 0;
    for (const auto& line : lines)
        textBlockWidth = std::max(textBlockWidth, fm.horizontalAdvance(line));

    int textBlockHeight = lines.size() * lineHeight;

    QRectF bgRect(tileRect.left() + padding, tileRect.top() + padding,
                  textBlockWidth + 2 * padding, textBlockHeight + 2 * padding);
    painter.fillRect(bgRect, QColor(0, 0, 0, 128));

    painter.setPen(Qt::white);
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

    painter.setPen(QPen(QColor(255, 255, 255, 128), 3.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(boundsRect);
}

void MapViewport::drawCenterOverlay(QPainter& painter, QPointF viewCenter)
{
    if (!m_showCenter || !m_center)
        return;

    QPointF screenPos = geoToScreen(m_center->longitude, m_center->latitude, viewCenter);

    painter.setPen(QPen(QColor(255, 255, 255, 128), 3.0));
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

    if (m_tileFocusActive) {
        // In focus mode: lock zoom level, allow wide scale range
        m_scale = std::clamp(newScale, 0.5, 8.0);
    } else if (newScale >= 2.0 && m_zoom < m_provider->maxZoom()) {
        worldUnderCursor *= 2.0;
        transitionZoom(m_zoom + 1);
        emit zoomChanged(m_zoom);
    } else if (newScale < 1.0 && m_zoom > m_provider->minZoom()) {
        worldUnderCursor /= 2.0;
        transitionZoom(m_zoom - 1);
        emit zoomChanged(m_zoom);
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
    if (event->button() == Qt::LeftButton && m_tileFocusSelecting) {
        focusTileAt(event->pos());
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        m_dragStartPos = event->pos();
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
        if (m_crispScale != 0)
            m_zoomSettleTimer.start();
        update();
        event->accept();
    }
}

void MapViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        bool wasClick = (event->pos() - m_dragStartPos).manhattanLength() < 4;
        m_dragging = false;
        setCursor(m_tileFocusSelecting ? Qt::CrossCursor : Qt::ArrowCursor);

        if (wasClick && m_isVectorProvider && m_provider) {
            QPointF viewCenter(width() / 2.0, height() / 2.0);
            double scaledTileSize = m_displayTileSize * m_scale;

            if (m_tileFocusActive) {
                // In focus mode: only inspect features on the focused tile,
                // including buffer area features outside [0, tileSize]
                QRectF tileRect = tileScreenRect(m_focusedTile.x, m_focusedTile.y,
                                                  viewCenter, scaledTileSize);
                double bufferScreenSize = scaledTileSize * m_focusBufferRatio;
                QRectF expandedRect = tileRect.adjusted(-bufferScreenSize, -bufferScreenSize,
                                                         bufferScreenSize, bufferScreenSize);
                if (expandedRect.contains(QPointF(event->pos()))) {
                    // Convert screen pos to tile-local coords relative to tile origin
                    // (not the expanded rect). This means buffer features get coords outside [0, tileSize].
                    QPointF tileLocal = (QPointF(event->pos()) - tileRect.topLeft())
                                        / tileRect.width() * m_displayTileSize;
                    emit inspectRequested(m_focusedTile, tileLocal,
                                          static_cast<double>(m_displayTileSize), m_scale);
                }
            } else {
                auto key = tileAtScreenPos(event->pos());
                QRectF tileRect = tileScreenRect(key.x, key.y, viewCenter, scaledTileSize);
                QPointF tileLocal = (QPointF(event->pos()) - tileRect.topLeft())
                                    / tileRect.width() * m_displayTileSize;
                emit inspectRequested(key, tileLocal, static_cast<double>(m_displayTileSize),
                                      m_scale);
            }
        }
        event->accept();
    }
}

void MapViewport::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_provider || !m_isVectorProvider)
        return;

    QMenu menu(this);
    if (m_tileFocusActive) {
        menu.addAction("Exit tile focus", this, &MapViewport::exitTileFocus);
    } else {
        auto key = tileAtScreenPos(event->pos());
        auto* action = menu.addAction(
            QString("Focus tile %1/%2/%3").arg(key.zoom).arg(key.x).arg(key.y));
        connect(action, &QAction::triggered, this, [this, pos = event->pos()] {
            focusTileAt(pos);
        });
    }
    menu.exec(event->globalPos());
}

void MapViewport::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_tileFocusActive) {
        exitTileFocus();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
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

    // Re-render focused tile at current scale
    if (m_tileFocusActive) {
        auto result = m_provider->tileUnclipped(
            m_focusedTile.zoom, m_focusedTile.x, m_focusedTile.y, crispSize);
        if (result) {
            m_focusedTilePixmap = result->pixmap;
            m_focusBufferRatio = result->bufferRatio;
        }
    }

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

    // Clear inspect highlights on integer zoom change
    if (!m_inspectHighlights.isEmpty()) {
        m_inspectHighlights.clear();
        m_inspectTileSize = 0;
        m_isolatedHighlight = -1;
        emit inspectCleared();
    }
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

// --- Feature inspection ---

void MapViewport::setInspectHighlights(TileKey tile, double tileSize,
                                        const QList<mvt::FeatureHighlight>& highlights)
{
    m_inspectTile = tile;
    m_inspectTileSize = tileSize;
    m_inspectHighlights = highlights;
    m_isolatedHighlight = -1;
    update();
}

void MapViewport::clearInspectHighlights()
{
    if (m_inspectHighlights.isEmpty())
        return;
    m_inspectHighlights.clear();
    m_inspectTileSize = 0;
    m_isolatedHighlight = -1;
    update();
}

void MapViewport::isolateInspectHighlight(int index)
{
    m_isolatedHighlight = index;
    update();
}

void MapViewport::removeInspectHighlightsForLayers(const QSet<QString>& layers)
{
    m_inspectHighlights.erase(
        std::remove_if(m_inspectHighlights.begin(), m_inspectHighlights.end(),
                        [&](const mvt::FeatureHighlight& h) {
                            return layers.contains(h.layerName);
                        }),
        m_inspectHighlights.end());
    m_isolatedHighlight = -1;

    if (m_inspectHighlights.isEmpty()) {
        m_inspectTileSize = 0;
        emit inspectCleared();
    }
    update();
}

void MapViewport::drawInspectHighlights(QPainter& painter, QPointF viewCenter,
                                         double scaledTileSize)
{
    QRectF tileRect = tileScreenRect(m_inspectTile.x, m_inspectTile.y,
                                      viewCenter, scaledTileSize);
    double screenScale = tileRect.width() / m_inspectTileSize;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(tileRect.topLeft());
    painter.scale(screenScale, screenScale);

    for (int i = 0; i < m_inspectHighlights.size(); ++i) {
        if (m_isolatedHighlight >= 0 && i != m_isolatedHighlight)
            continue;

        const auto& f = m_inspectHighlights[i];
        QColor color = f.color;

        switch (f.type) {
        case mvt::GeomType::Polygon: {
            QColor fillColor = color;
            fillColor.setAlpha(80);
            QColor strokeColor = color;
            strokeColor.setAlpha(220);
            painter.setBrush(fillColor);
            painter.setPen(QPen(strokeColor, 2.0 / screenScale));
            painter.drawPath(f.path);
            break;
        }
        case mvt::GeomType::LineString: {
            QColor strokeColor = color;
            strokeColor.setAlpha(220);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(strokeColor, 3.0 / screenScale));
            painter.drawPath(f.path);
            break;
        }
        case mvt::GeomType::Point: {
            QColor dotColor = color;
            dotColor.setAlpha(220);
            painter.setPen(Qt::NoPen);
            painter.setBrush(dotColor);
            double r = 5.0 / screenScale;
            for (const auto& pt : f.points)
                painter.drawEllipse(pt, r, r);
            break;
        }
        default:
            break;
        }
    }

    painter.restore();
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
