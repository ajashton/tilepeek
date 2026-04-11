#pragma once

#include <QMainWindow>
#include <QSet>
#include <memory>

class EmptyStateWidget;
class MapViewport;
class MetadataSidebar;
class TileProvider;
class TileStatsWorker;
class ToastManager;
class QActionGroup;
class QMenu;
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
    void onTileScaleChanged(QAction* action);
    void populateTileScaleMenu(bool isVector);
    void stopStatsThread();

    MapViewport* m_mapViewport = nullptr;
    MetadataSidebar* m_sidebar = nullptr;
    ToastManager* m_toastManager = nullptr;
    EmptyStateWidget* m_emptyState = nullptr;
    QStackedWidget* m_stack = nullptr;
    std::unique_ptr<TileProvider> m_tileProvider;

    QThread* m_statsThread = nullptr;

    QMenu* m_tileScaleMenu = nullptr;
    QActionGroup* m_tileScaleGroup = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_tileFocusAction = nullptr;
    int m_nativeTileSize = 256;
    bool m_isVectorFormat = false;
};
