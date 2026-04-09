#pragma once

#include "map/TileCache.h"
#include "map/TileProvider.h"
#include "mbtiles/MBTilesMetadataParser.h"

#include <QPointF>
#include <QTimer>
#include <QWidget>
#include <optional>
#include <unordered_map>

class MapViewport : public QWidget {
    Q_OBJECT
public:
    explicit MapViewport(QWidget* parent = nullptr);

    void setTileProvider(TileProvider* provider);
    void setView(double longitude, double latitude, int zoom);
    void clear();

    void setShowTileBoundaries(bool on);
    void setShowTileIds(bool on);
    void setShowTileSizes(bool on);
    void setShowBounds(bool on);
    void setShowCenter(bool on);

    void setBounds(std::optional<ParsedBounds> bounds);
    void setCenter(std::optional<ParsedCenter> center);
    void setBackgroundColor(const QColor& color);
    void setDisplayTileSize(int size);
    void clearTileCache();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void clampViewport();
    void transitionZoom(int newZoom);
    double displayScale() const { return m_displayTileSize / 256.0; }
    QRect visibleTileRange() const;
    QPixmap fetchTile(int zoom, int x, int y);
    std::optional<int> fetchTileSize(int zoom, int x, int y);
    QRectF tileScreenRect(int tx, int ty, QPointF viewCenter, double scaledTileSize) const;
    QPointF geoToScreen(double lon, double lat, QPointF viewCenter) const;

    void drawMissingTile(QPainter& painter, const QRectF& tileRect);
    void drawTileOverlays(QPainter& painter, const QRect& tiles,
                          QPointF viewCenter, double scaledTileSize);
    void drawTileText(QPainter& painter, const QRectF& tileRect, const QStringList& lines);
    void drawBoundsOverlay(QPainter& painter, QPointF viewCenter);
    void drawCenterOverlay(QPainter& painter, QPointF viewCenter);
    void onZoomSettled();

    TileProvider* m_provider = nullptr;
    TileCache m_cache;
    std::unordered_map<TileKey, std::optional<int>, TileKeyHash> m_tileSizeCache;

    int m_zoom = 0;
    double m_scale = 1.0;
    QPointF m_centerPixel;

    bool m_dragging = false;
    QPoint m_lastMousePos;

    bool m_showTileBoundaries = false;
    bool m_showTileIds = false;
    bool m_showTileSizes = false;
    bool m_showBounds = false;
    bool m_showCenter = false;

    std::optional<ParsedBounds> m_bounds;
    std::optional<ParsedCenter> m_center;
    QColor m_bgColor;
    int m_displayTileSize = 256;

    QTimer m_zoomSettleTimer;
    TileCache m_crispCache;
    double m_crispScale = 0;
};
