#include "app/MainWindow.h"
#include "map/MapViewport.h"
#include "map/RasterTileProvider.h"
#include "mbtiles/MBTilesMetadataParser.h"
#include "mbtiles/MBTilesReader.h"
#include "widgets/MetadataSidebar.h"
#include "widgets/ToastManager.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMimeData>
#include <QSplitter>

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

MainWindow::~MainWindow() = default;

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

    m_sidebar = new MetadataSidebar(splitter);
    m_mapViewport = new MapViewport(splitter);

    splitter->addWidget(m_sidebar);
    splitter->addWidget(m_mapViewport);
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

    auto reader = std::make_unique<MBTilesReader>(path);
    if (!reader->open()) {
        m_toastManager->showError("Failed to open file: " + path);
        return;
    }

    auto validation = reader->validateSchema();
    for (const auto& error : validation.errors)
        m_toastManager->showError(error);

    if (!validation.metadataTableValid || !validation.tilesTableValid)
        return;

    auto rawMetadata = reader->readRawMetadata();
    auto zoomRange = reader->queryZoomRange();

    auto [metadata, messages] = MBTilesMetadataParser::parse(rawMetadata, zoomRange);

    m_sidebar->setMetadata(metadata);

    for (const auto& msg : messages) {
        if (msg.level == ValidationMessage::Level::Error)
            m_toastManager->showError(msg.text);
        else
            m_toastManager->showWarning(msg.text);
    }

    // Extract format and zoom range for tile provider
    auto formatOpt = metadata.value("format");
    QString format = formatOpt.value_or("png");

    int minZoom = 0;
    int maxZoom = 0;
    if (zoomRange) {
        minZoom = zoomRange->minZoom;
        maxZoom = zoomRange->maxZoom;
    }
    if (auto v = metadata.value("minzoom"))
        minZoom = v->toInt();
    if (auto v = metadata.value("maxzoom"))
        maxZoom = v->toInt();

    // Create tile provider and validate format
    auto provider = std::make_unique<RasterTileProvider>(
        std::move(reader), format, minZoom, maxZoom);

    auto formatResult = provider->validateFormat();
    switch (formatResult.status) {
    case FormatValidationResult::Status::Unsupported:
        m_toastManager->showError(formatResult.message);
        setWindowTitle("TilePeek - " + QFileInfo(path).fileName());
        return;
    case FormatValidationResult::Status::UnrecognizedFormat:
        m_toastManager->showError(formatResult.message);
        setWindowTitle("TilePeek - " + QFileInfo(path).fileName());
        return;
    case FormatValidationResult::Status::FormatMismatch:
        m_toastManager->showWarning(formatResult.message);
        break;
    case FormatValidationResult::Status::Ok:
        break;
    }

    m_tileProvider = std::move(provider);
    m_mapViewport->setTileProvider(m_tileProvider.get());

    // Set initial view: lowest zoom, centered on center metadata point
    auto centerOpt = MBTilesMetadataParser::parseCenter(metadata.value("center").value_or(""));
    if (centerOpt) {
        m_mapViewport->setView(centerOpt->longitude, centerOpt->latitude, minZoom);
    } else {
        m_mapViewport->setView(0.0, 0.0, minZoom);
    }

    setWindowTitle("TilePeek - " + QFileInfo(path).fileName());
}

void MainWindow::clearCurrentFile()
{
    m_mapViewport->clear();
    m_tileProvider.reset();
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
