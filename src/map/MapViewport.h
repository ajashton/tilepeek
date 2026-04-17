#pragma once

#include "map/TileCache.h"
#include "map/TileProvider.h"
#include "mbtiles/MBTilesMetadataParser.h"
#include "mvt/FeatureHitTest.h"

#include <QPointF>
#include <QTimer>
#include <QWidget>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class MapViewport : public QWidget {
    Q_OBJECT
public:
    explicit MapViewport(QWidget* parent = nullptr);

    void setTileProvider(std::shared_ptr<TileProvider> provider);
    void setView(double longitude, double latitude, int zoom);
    void clear();

    void zoomIn();
    void zoomOut();
    void setZoom(int zoom);
    void zoomToBounds(double left, double bottom, double right, double top);
    int currentZoom() const { return m_zoom; }

    void setTileFocusSelecting(bool on);
    void focusTileAt(const QPoint& screenPos);
    void exitTileFocus();
    bool isTileFocusActive() const { return m_tileFocusActive; }
    bool isVectorProvider() const { return m_isVectorProvider; }
    void setVectorProvider(bool isVector) { m_isVectorProvider = isVector; }

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

    void setInspectHighlights(TileKey tile, double tileSize,
                              const QList<mvt::FeatureHighlight>& highlights);
    void clearInspectHighlights();
    void isolateInspectHighlight(int index);
    void removeInspectHighlightsForLayers(const QSet<QString>& layers);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

signals:
    void tileFocusChanged(bool active);
    void zoomChanged(int zoom);
    void inspectRequested(TileKey tile, QPointF tileLocalPos, double tileSize, double scale);
    void inspectCleared();

private:
    TileKey tileAtScreenPos(const QPoint& pos) const;
    void clampViewport();
    void transitionZoom(int newZoom);
    double displayScale() const { return m_displayTileSize / 256.0; }
    QRect visibleTileRange() const;
    std::optional<int> fetchTileSize(int zoom, int x, int y);
    QRectF tileScreenRect(int tx, int ty, QPointF viewCenter, double scaledTileSize) const;
    QPointF geoToScreen(double lon, double lat, QPointF viewCenter) const;

    void drawMissingTile(QPainter& painter, const QRectF& tileRect);
    bool drawFallback(QPainter& painter, const QRectF& tileRect, int zoom, int x, int y);
    void drawTileOverlays(QPainter& painter, const QRect& tiles,
                          QPointF viewCenter, double scaledTileSize);
    void drawTileText(QPainter& painter, const QRectF& tileRect, const QStringList& lines);
    void drawBoundsOverlay(QPainter& painter, QPointF viewCenter);
    void drawCenterOverlay(QPainter& painter, QPointF viewCenter);
    void drawInspectHighlights(QPainter& painter, QPointF viewCenter, double scaledTileSize);
    void onZoomSettled();

    // Async tile production. All four dispatch work to the global QThreadPool
    // and deliver results on the UI thread via QFutureWatcher.
    void requestTileAsync(int zoom, int x, int y);
    void requestCrispAsync(int zoom, int x, int y, int crispSize);
    void requestUnclippedAsync(TileKey key, int size);
    void cancelAllInFlight();

    std::shared_ptr<TileProvider> m_provider;
    TileCache m_cache;
    std::unordered_map<TileKey, std::optional<int>, TileKeyHash> m_tileSizeCache;

    // Tiles currently being rendered off-thread. Keyed on TileKey for the base
    // cache path and on TileKey for the crisp cache path; dedupes repeat paint
    // requests during a pan so we don't queue duplicate work.
    std::unordered_set<TileKey, TileKeyHash> m_inFlightBase;
    std::unordered_set<TileKey, TileKeyHash> m_inFlightCrisp;
    bool m_inFlightUnclipped = false;

    int m_zoom = 0;
    double m_scale = 1.0;
    QPointF m_centerPixel;
    qreal m_dpr = 1.0;

    bool m_dragging = false;
    QPoint m_lastMousePos;
    QPoint m_dragStartPos;

    bool m_showTileBoundaries = false;
    bool m_showTileIds = false;
    bool m_showTileSizes = false;
    bool m_showBounds = false;
    bool m_showCenter = false;

    std::optional<ParsedBounds> m_bounds;
    std::optional<ParsedCenter> m_center;
    QColor m_bgColor;
    int m_displayTileSize = 256;
    bool m_isVectorProvider = false;

    QTimer m_zoomSettleTimer;
    TileCache m_crispCache;
    double m_crispScale = 0;

    // Tile focus mode
    bool m_tileFocusActive = false;
    bool m_tileFocusSelecting = false;
    TileKey m_focusedTile{};
    QPixmap m_focusedTilePixmap;
    double m_focusBufferRatio = 0.0;

    // Feature inspection highlights
    TileKey m_inspectTile{};
    QList<mvt::FeatureHighlight> m_inspectHighlights;
    double m_inspectTileSize = 0;
    int m_isolatedHighlight = -1; // -1 = show all
};
