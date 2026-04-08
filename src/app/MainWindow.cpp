#include "app/MainWindow.h"
#include "map/MapViewport.h"
#include "map/RasterTileProvider.h"
#include "map/VectorTileProvider.h"
#include "mbtiles/MBTilesMetadataParser.h"
#include "mbtiles/MBTilesReader.h"
#include "mbtiles/VectorMetadataParser.h"
#include "model/TileStatistics.h"
#include "stats/TileStatsWorker.h"
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
#include <QThread>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("TilePeek");
    resize(1000, 700);
    setAcceptDrops(true);

    setupCentralWidget();
    setupMenuBar();

    m_toastManager = new ToastManager(this);

    qRegisterMetaType<TileStatistics>("TileStatistics");
}

MainWindow::~MainWindow()
{
    stopStatsThread();
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

    // View menu
    auto* viewMenu = menuBar()->addMenu("&View");

    auto* showBoundaries = viewMenu->addAction("Show Tile &Boundaries");
    showBoundaries->setCheckable(true);
    connect(showBoundaries, &QAction::toggled, m_mapViewport, &MapViewport::setShowTileBoundaries);

    auto* showTileIds = viewMenu->addAction("Show Tile &IDs");
    showTileIds->setCheckable(true);
    connect(showTileIds, &QAction::toggled, m_mapViewport, &MapViewport::setShowTileIds);

    auto* showTileSizes = viewMenu->addAction("Show Tile &Sizes");
    showTileSizes->setCheckable(true);
    connect(showTileSizes, &QAction::toggled, m_mapViewport, &MapViewport::setShowTileSizes);

    viewMenu->addSeparator();

    auto* showBoundsBox = viewMenu->addAction("Show B&ounds Box");
    showBoundsBox->setCheckable(true);
    connect(showBoundsBox, &QAction::toggled, m_mapViewport, &MapViewport::setShowBounds);

    auto* showCenterPt = viewMenu->addAction("Show &Center Point");
    showCenterPt->setCheckable(true);
    connect(showCenterPt, &QAction::toggled, m_mapViewport, &MapViewport::setShowCenter);
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

    for (const auto& msg : messages) {
        if (msg.level == ValidationMessage::Level::Error)
            m_toastManager->showError(msg.text);
        else
            m_toastManager->showWarning(msg.text);
    }

    // Extract format and zoom range
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

    if (format == "pbf") {
        // Vector tile path
        auto jsonStr = metadata.value("json");
        if (!jsonStr) {
            m_toastManager->showError("Missing required 'json' metadata key for pbf format");
            m_sidebar->setMetadata(metadata);
        } else {
            auto vResult = VectorMetadataParser::parse(*jsonStr);
            for (const auto& vmsg : vResult.messages) {
                if (vmsg.level == ValidationMessage::Level::Error)
                    m_toastManager->showError(vmsg.text);
                else
                    m_toastManager->showWarning(vmsg.text);
            }
            if (vResult.metadata)
                m_sidebar->setVectorMetadata(metadata, *vResult.metadata);
            else
                m_sidebar->setMetadata(metadata);
        }

        m_tileProvider = std::make_unique<VectorTileProvider>(
            std::move(reader), minZoom, maxZoom);
    } else {
        // Raster tile path
        auto provider = std::make_unique<RasterTileProvider>(
            std::move(reader), format, minZoom, maxZoom);

        auto formatResult = provider->validateFormat();
        switch (formatResult.status) {
        case FormatValidationResult::Status::Unsupported:
            m_toastManager->showError(formatResult.message);
            m_sidebar->setMetadata(metadata);
            setWindowTitle("TilePeek - " + QFileInfo(path).fileName());
            return;
        case FormatValidationResult::Status::UnrecognizedFormat:
            m_toastManager->showError(formatResult.message);
            m_sidebar->setMetadata(metadata);
            setWindowTitle("TilePeek - " + QFileInfo(path).fileName());
            return;
        case FormatValidationResult::Status::FormatMismatch:
            m_toastManager->showWarning(formatResult.message);
            break;
        case FormatValidationResult::Status::Ok:
            break;
        }

        m_sidebar->setMetadata(metadata);
        m_tileProvider = std::move(provider);
    }

    m_mapViewport->setTileProvider(m_tileProvider.get());

    // Pass bounds and center to viewport
    auto boundsOpt = MBTilesMetadataParser::parseBounds(metadata.value("bounds").value_or(""));
    auto centerOpt = MBTilesMetadataParser::parseCenter(metadata.value("center").value_or(""));

    m_mapViewport->setBounds(boundsOpt);
    m_mapViewport->setCenter(centerOpt);

    if (centerOpt) {
        m_mapViewport->setView(centerOpt->longitude, centerOpt->latitude, minZoom);
    } else {
        m_mapViewport->setView(0.0, 0.0, minZoom);
    }

    setWindowTitle("TilePeek - " + QFileInfo(path).fileName());

    // Start async tile statistics
    m_sidebar->setStatsPlaceholder();

    auto* thread = new QThread(this);
    auto* worker = new TileStatsWorker(path);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &TileStatsWorker::calculate);
    connect(worker, &TileStatsWorker::finished, this, &MainWindow::onStatsReady);
    connect(worker, &TileStatsWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    m_statsThread = thread;
    m_statsThread->start();
}

void MainWindow::onStatsReady(TileStatistics stats)
{
    m_sidebar->setTileStatistics(stats);
    m_statsThread = nullptr;
}

void MainWindow::stopStatsThread()
{
    if (m_statsThread && m_statsThread->isRunning()) {
        m_statsThread->quit();
        m_statsThread->wait(2000);
    }
    m_statsThread = nullptr;
}

void MainWindow::clearCurrentFile()
{
    stopStatsThread();
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
