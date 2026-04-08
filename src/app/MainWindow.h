#pragma once

#include <QMainWindow>

class MetadataSidebar;
class ToastManager;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

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

    QWidget* m_mapPlaceholder = nullptr;
    MetadataSidebar* m_sidebar = nullptr;
    ToastManager* m_toastManager = nullptr;
};
