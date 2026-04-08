#pragma once

#include "map/TileCache.h"
#include "map/TileProvider.h"

#include <QPointF>
#include <QWidget>

class MapViewport : public QWidget {
    Q_OBJECT
public:
    explicit MapViewport(QWidget* parent = nullptr);

    void setTileProvider(TileProvider* provider);
    void setView(double longitude, double latitude, int zoom);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void clampViewport();
    void transitionZoom(int newZoom);
    QRect visibleTileRange() const;
    QPixmap fetchTile(int zoom, int x, int y);

    TileProvider* m_provider = nullptr;
    TileCache m_cache;

    int m_zoom = 0;
    double m_scale = 1.0;
    QPointF m_centerPixel;

    bool m_dragging = false;
    QPoint m_lastMousePos;
};
