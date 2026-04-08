#include "app/MainWindow.h"
#include "mbtiles/MBTilesMetadataParser.h"
#include "mbtiles/MBTilesReader.h"
#include "widgets/MetadataSidebar.h"
#include "widgets/ToastManager.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMimeData>
#include <QSplitter>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("TilePeek");
    resize(1000, 700);
    setAcceptDrops(true);

    setupMenuBar();
    setupCentralWidget();

    m_toastManager = new ToastManager(this);
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    fileMenu->addSeparator();

    auto* quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::setupCentralWidget()
{
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    m_mapPlaceholder = new QWidget(splitter);
    m_mapPlaceholder->setStyleSheet("background-color: #f0f0f0;");
    auto* placeholderLabel = new QLabel("Map view will appear here", m_mapPlaceholder);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet("color: #999; font-size: 16px;");
    auto* placeholderLayout = new QVBoxLayout(m_mapPlaceholder);
    placeholderLayout->addWidget(placeholderLabel);

    m_sidebar = new MetadataSidebar(splitter);

    splitter->addWidget(m_sidebar);
    splitter->addWidget(m_mapPlaceholder);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({300, 700});

    setCentralWidget(splitter);
}

void MainWindow::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open MBTiles File", QString(), "MBTiles Files (*.mbtiles);;All Files (*)");
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::openFile(const QString& path)
{
    loadMBTiles(path);
}

void MainWindow::loadMBTiles(const QString& path)
{
    clearCurrentFile();

    MBTilesReader reader(path);
    if (!reader.open()) {
        m_toastManager->showError("Failed to open file: " + path);
        return;
    }

    auto validation = reader.validateSchema();
    for (const auto& error : validation.errors)
        m_toastManager->showError(error);

    if (!validation.metadataTableValid || !validation.tilesTableValid)
        return;

    auto rawMetadata = reader.readRawMetadata();
    auto zoomRange = reader.queryZoomRange();

    auto [metadata, messages] = MBTilesMetadataParser::parse(rawMetadata, zoomRange);

    m_sidebar->setMetadata(metadata);

    for (const auto& msg : messages) {
        if (msg.level == ValidationMessage::Level::Error)
            m_toastManager->showError(msg.text);
        else
            m_toastManager->showWarning(msg.text);
    }

    setWindowTitle("TilePeek - " + QFileInfo(path).fileName());
}

void MainWindow::clearCurrentFile()
{
    m_sidebar->clear();
    m_toastManager->clearAll();
    setWindowTitle("TilePeek");
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        for (const auto& url : event->mimeData()->urls()) {
            if (url.isLocalFile()
                && url.toLocalFile().endsWith(".mbtiles", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    for (const auto& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && url.toLocalFile().endsWith(".mbtiles", Qt::CaseInsensitive)) {
            openFile(url.toLocalFile());
            return;
        }
    }
}
