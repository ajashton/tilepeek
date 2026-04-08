#include "map/MapViewport.h"
#include "map/WebMercator.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

MapViewport::MapViewport(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("background-color: #e0e0e0;");
}

void MapViewport::setTileProvider(TileProvider* provider)
{
    m_provider = provider;
    m_cache.clear();
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
    m_zoom = 0;
    m_scale = 1.0;
    m_centerPixel = QPointF();
    update();
}

void MapViewport::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#e0e0e0"));

    if (!m_provider)
        return;

    double scaledTileSize = WebMercator::TileSize * m_scale;
    QPointF viewCenter(width() / 2.0, height() / 2.0);

    QRect tiles = visibleTileRange();

    for (int ty = tiles.top(); ty <= tiles.bottom(); ++ty) {
        for (int tx = tiles.left(); tx <= tiles.right(); ++tx) {
            QPointF worldPixel(tx * WebMercator::TileSize, ty * WebMercator::TileSize);
            QPointF offset = (worldPixel - m_centerPixel) * m_scale;
            QPointF screenPos = viewCenter + offset;
            QRectF tileRect(screenPos, QSizeF(scaledTileSize, scaledTileSize));

            QPixmap pixmap = fetchTile(m_zoom, tx, ty);
            if (!pixmap.isNull()) {
                painter.drawPixmap(tileRect, pixmap, pixmap.rect());
            } else {
                painter.fillRect(tileRect, QColor("#d0d0d0"));
                painter.setPen(QColor("#bbb"));
                painter.drawRect(tileRect);
            }
        }
    }
}

void MapViewport::wheelEvent(QWheelEvent* event)
{
    if (!m_provider)
        return;

    double angleDelta = event->angleDelta().y();
    if (angleDelta == 0)
        return;

    double scaleStep = 1.0 + (angleDelta / 120.0) * 0.1;
    double newScale = m_scale * scaleStep;

    if (newScale >= 2.0 && m_zoom < m_provider->maxZoom()) {
        transitionZoom(m_zoom + 1);
    } else if (newScale < 1.0 && m_zoom > m_provider->minZoom()) {
        transitionZoom(m_zoom - 1);
    } else if (newScale >= 2.0) {
        // At maxZoom — clamp
        m_scale = 1.99;
    } else if (newScale < 1.0) {
        // At minZoom — clamp
        m_scale = 1.0;
    } else {
        m_scale = newScale;
    }

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
        m_centerPixel -= QPointF(delta) / m_scale;
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
}

QRect MapViewport::visibleTileRange() const
{
    double halfViewW = (width() / 2.0) / m_scale;
    double halfViewH = (height() / 2.0) / m_scale;

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
