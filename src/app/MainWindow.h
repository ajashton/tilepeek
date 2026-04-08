#pragma once

#include <QMainWindow>
#include <memory>

class MapViewport;
class MetadataSidebar;
class RasterTileProvider;
class TileStatsWorker;
class ToastManager;
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
    void setupCentralWidget();
    void onOpenFile();
    void loadMBTiles(const QString& path);
    void clearCurrentFile();
    void onStatsReady(TileStatistics stats);
    void stopStatsThread();

    MapViewport* m_mapViewport = nullptr;
    MetadataSidebar* m_sidebar = nullptr;
    ToastManager* m_toastManager = nullptr;
    std::unique_ptr<RasterTileProvider> m_tileProvider;

    QThread* m_statsThread = nullptr;
};
