#include "app/MainWindow.h"
#include "map/MapViewport.h"
#include "map/RasterTileProvider.h"
#include "map/VectorTileProvider.h"
#include "mbtiles/MBTilesMetadataParser.h"
#include "mbtiles/MBTilesReader.h"
#include "mbtiles/VectorMetadataParser.h"
#include "model/TileStatistics.h"
#include "pmtiles/PMTilesMetadataParser.h"
#include "pmtiles/PMTilesReader.h"
#include "stats/TileStatsWorker.h"
#include "widgets/MetadataSidebar.h"
#include "widgets/ToastManager.h"

#include <QAction>
#include <QActionGroup>
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

    viewMenu->addSeparator();

    m_tileScaleMenu = viewMenu->addMenu("Tile &Scale");
    m_tileScaleMenu->setEnabled(false);
    m_tileScaleGroup = new QActionGroup(this);
    m_tileScaleGroup->setExclusive(true);
    connect(m_tileScaleGroup, &QActionGroup::triggered, this, &MainWindow::onTileScaleChanged);
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
        this, "Open Tile Archive", QString(),
        "Tile Archives (*.mbtiles *.pmtiles);;MBTiles Files (*.mbtiles);;PMTiles Files (*.pmtiles);;All Files (*)");
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::openFile(const QString& path)
{
    if (path.endsWith(".pmtiles", Qt::CaseInsensitive))
        loadPMTiles(path);
    else
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
        QStringList layerNames;
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
            if (vResult.metadata) {
                m_sidebar->setVectorMetadata(metadata, *vResult.metadata);
                for (const auto& layer : vResult.metadata->vectorLayers)
                    layerNames << layer.id;
            } else {
                m_sidebar->setMetadata(metadata);
            }
        }

        auto vectorProvider = std::make_unique<VectorTileProvider>(
            std::move(reader), minZoom, maxZoom, layerNames);
        vectorProvider->setRenderSize(512);
        m_tileProvider = std::move(vectorProvider);
        m_mapViewport->setBackgroundColor(QColor("#1a1a2e"));
        m_mapViewport->setDisplayTileSize(512);
        populateTileScaleMenu(true);

        connect(m_sidebar, &MetadataSidebar::layerVisibilityChanged,
                this, &MainWindow::onLayerVisibilityChanged);
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
        m_nativeTileSize = provider->detectNativeTileSize();
        m_mapViewport->setDisplayTileSize(m_nativeTileSize);
        m_tileProvider = std::move(provider);
        populateTileScaleMenu(false);
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

void MainWindow::loadPMTiles(const QString& path)
{
    clearCurrentFile();

    auto reader = std::make_unique<PMTilesReader>(path);
    if (!reader->open()) {
        m_toastManager->showError("Failed to open file: " + path);
        return;
    }

    auto validation = reader->validate();
    for (const auto& error : validation.errors)
        m_toastManager->showError(error);
    if (!validation.valid)
        return;

    const auto& header = reader->header();
    auto jsonMeta = reader->readJsonMetadata();
    auto [metadata, messages] = PMTilesMetadataParser::parse(header, jsonMeta);

    for (const auto& msg : messages) {
        if (msg.level == ValidationMessage::Level::Error)
            m_toastManager->showError(msg.text);
        else
            m_toastManager->showWarning(msg.text);
    }

    QString format = PMTilesMetadataParser::tileTypeToFormat(header.tile_type);
    int minZoom = header.min_zoom;
    int maxZoom = header.max_zoom;

    if (format == "pbf") {
        // Vector tile path
        QStringList layerNames;
        auto jsonStr = metadata.value("json");
        if (!jsonStr) {
            if (!jsonMeta.isEmpty())
                m_toastManager->showWarning("No vector_layers found in PMTiles metadata");
            m_sidebar->setMetadata(metadata);
        } else {
            auto vResult = VectorMetadataParser::parse(*jsonStr);
            for (const auto& vmsg : vResult.messages) {
                if (vmsg.level == ValidationMessage::Level::Error)
                    m_toastManager->showError(vmsg.text);
                else
                    m_toastManager->showWarning(vmsg.text);
            }
            if (vResult.metadata) {
                m_sidebar->setVectorMetadata(metadata, *vResult.metadata);
                for (const auto& layer : vResult.metadata->vectorLayers)
                    layerNames << layer.id;
            } else {
                m_sidebar->setMetadata(metadata);
            }
        }

        auto vectorProvider = std::make_unique<VectorTileProvider>(
            std::move(reader), minZoom, maxZoom, layerNames);
        vectorProvider->setRenderSize(512);
        m_tileProvider = std::move(vectorProvider);
        m_mapViewport->setBackgroundColor(QColor("#1a1a2e"));
        m_mapViewport->setDisplayTileSize(512);
        populateTileScaleMenu(true);

        connect(m_sidebar, &MetadataSidebar::layerVisibilityChanged,
                this, &MainWindow::onLayerVisibilityChanged);
    } else {
        // Raster tile path
        auto provider = std::make_unique<RasterTileProvider>(
            std::move(reader), format, minZoom, maxZoom);

        m_sidebar->setMetadata(metadata);
        m_nativeTileSize = provider->detectNativeTileSize();
        m_mapViewport->setDisplayTileSize(m_nativeTileSize);
        m_tileProvider = std::move(provider);
        populateTileScaleMenu(false);
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

void MainWindow::onLayerVisibilityChanged(const QSet<QString>& hiddenLayers)
{
    if (auto* vtp = dynamic_cast<VectorTileProvider*>(m_tileProvider.get())) {
        vtp->setHiddenLayers(hiddenLayers);
        m_mapViewport->clearTileCache();
    }
}

void MainWindow::populateTileScaleMenu(bool isVector)
{
    // Clear existing actions
    for (auto* action : m_tileScaleGroup->actions())
        m_tileScaleGroup->removeAction(action);
    m_tileScaleMenu->clear();

    m_isVectorFormat = isVector;

    if (isVector) {
        struct Preset { const char* label; int size; };
        constexpr Preset presets[] = {{"256px", 256}, {"512px", 512}, {"1024px", 1024}};
        for (auto [label, size] : presets) {
            auto* action = m_tileScaleMenu->addAction(label);
            action->setCheckable(true);
            action->setData(size);
            m_tileScaleGroup->addAction(action);
            if (size == 512)
                action->setChecked(true);
        }
    } else {
        struct Preset { const char* label; int factor; };
        constexpr Preset presets[] = {
            {"1\xc3\x97 (Native)", 1}, {"2\xc3\x97", 2}, {"3\xc3\x97", 3}};
        for (auto [label, factor] : presets) {
            auto* action = m_tileScaleMenu->addAction(label);
            action->setCheckable(true);
            action->setData(factor);
            m_tileScaleGroup->addAction(action);
            if (factor == 1)
                action->setChecked(true);
        }
    }

    m_tileScaleMenu->setEnabled(true);
}

void MainWindow::onTileScaleChanged(QAction* action)
{
    int displayTileSize;

    if (m_isVectorFormat) {
        displayTileSize = action->data().toInt();
        if (auto* vtp = dynamic_cast<VectorTileProvider*>(m_tileProvider.get())) {
            vtp->setRenderSize(displayTileSize);
            m_mapViewport->clearTileCache();
        }
    } else {
        int scaleFactor = action->data().toInt();
        displayTileSize = m_nativeTileSize / scaleFactor;
    }

    m_mapViewport->setDisplayTileSize(displayTileSize);
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
    disconnect(m_sidebar, &MetadataSidebar::layerVisibilityChanged,
               this, &MainWindow::onLayerVisibilityChanged);
    m_mapViewport->clear();
    m_mapViewport->setBackgroundColor(palette().color(QPalette::Window));
    m_tileProvider.reset();
    m_sidebar->clear();
    m_toastManager->clearAll();
    m_tileScaleMenu->setEnabled(false);
    for (auto* action : m_tileScaleGroup->actions())
        m_tileScaleGroup->removeAction(action);
    m_tileScaleMenu->clear();
    m_nativeTileSize = 256;
    m_isVectorFormat = false;
    setWindowTitle("TilePeek");
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        for (const auto& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                auto file = url.toLocalFile();
                if (file.endsWith(".mbtiles", Qt::CaseInsensitive)
                    || file.endsWith(".pmtiles", Qt::CaseInsensitive)) {
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    for (const auto& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            auto file = url.toLocalFile();
            if (file.endsWith(".mbtiles", Qt::CaseInsensitive)
                || file.endsWith(".pmtiles", Qt::CaseInsensitive)) {
                openFile(file);
                return;
            }
        }
    }
}
