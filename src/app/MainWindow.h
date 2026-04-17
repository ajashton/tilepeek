#pragma once

#include "map/TileCache.h"
#include "mbtiles/MBTilesMetadataParser.h"

#include <QMainWindow>
#include <QPointF>
#include <QSet>
#include <memory>

class EmptyStateWidget;
class MapViewport;
class MetadataSidebar;
class TileProvider;
class TileStatsWorker;
class QActionGroup;
class QMenu;
class QSlider;
class QStackedWidget;
class QThread;
struct TileStatistics;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openFile(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void onOpenFile();
    void loadMBTiles(const QString& path);
    void loadPMTiles(const QString& path);
    void clearCurrentFile();
    void onStatsReady(TileStatistics stats);
    void onLayerVisibilityChanged(const QSet<QString>& hiddenLayers);
    void onInspectRequested(TileKey tile, QPointF tileLocalPos, double tileSize, double scale);
    void onInspectCleared();
    void zoomToTilesetBounds();
    void onTileScaleChanged(QAction* action);
    void populateTileScaleMenu(bool isVector);
    void stopStatsThread();

    MapViewport* m_mapViewport = nullptr;
    MetadataSidebar* m_sidebar = nullptr;
    EmptyStateWidget* m_emptyState = nullptr;
    QStackedWidget* m_stack = nullptr;
    std::shared_ptr<TileProvider> m_tileProvider;

    QThread* m_statsThread = nullptr;

    QMenu* m_tileScaleMenu = nullptr;
    QActionGroup* m_tileScaleGroup = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_zoomToBoundsAction = nullptr;
    QAction* m_tileFocusAction = nullptr;
    QSlider* m_zoomSlider = nullptr;
    int m_nativeTileSize = 256;
    bool m_isVectorFormat = false;
    std::optional<ParsedBounds> m_tilesetBounds;
};
