// SPDX-FileCopyrightText: 2026 AJ Ashton <aj@ajashton.ca>
// SPDX-License-Identifier: GPL-3.0-only

#include "app/AboutDialog.h"
#include "app/MainWindow.h"
#include "map/MapViewport.h"
#include "map/RasterTileProvider.h"
#include "map/VectorTileProvider.h"
#include "map/WebMercator.h"
#include "mvt/FeatureHitTest.h"
#include "mvt/MvtGeometry.h"
#include "mbtiles/MBTilesMetadataParser.h"
#include "mbtiles/MBTilesReader.h"
#include "mbtiles/VectorMetadataParser.h"
#include "model/TileStatistics.h"
#include "pmtiles/PMTilesMetadataParser.h"
#include "pmtiles/PMTilesReader.h"
#include "stats/TileStatsWorker.h"
#include "util/CetColormap.h"
#include "widgets/EmptyStateWidget.h"
#include "widgets/MetadataSidebar.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QMimeData>
#include <QSlider>
#include <QSplitter>
#include <QToolBar>
#include <QStackedWidget>
#include <QThread>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("TilePeek"));
    QSettings settings;
    resize(settings.value("window/size", QSize(1100, 700)).toSize());
    setAcceptDrops(true);

    setupCentralWidget();
    setupMenuBar();
    setupToolBar();

    qRegisterMetaType<TileStatistics>("TileStatistics");
}

MainWindow::~MainWindow()
{
    stopStatsThread();
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    auto* openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    fileMenu->addSeparator();

    auto* quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // View menu
    auto* viewMenu = menuBar()->addMenu(tr("&View"));

    QSettings settings;

    auto addViewToggle = [&](const QString& label, const QString& key,
                             void (MapViewport::*setter)(bool)) {
        auto* action = viewMenu->addAction(label);
        action->setCheckable(true);
        connect(action, &QAction::toggled, m_mapViewport, setter);
        connect(action, &QAction::toggled, this, [key](bool on) {
            QSettings().setValue(key, on);
        });
        action->setChecked(settings.value(key, false).toBool());
        return action;
    };

    addViewToggle(tr("Show Tile &Boundaries"), "view/showTileBoundaries",
                  &MapViewport::setShowTileBoundaries);
    addViewToggle(tr("Show Tile &IDs"), "view/showTileIds",
                  &MapViewport::setShowTileIds);
    addViewToggle(tr("Show Tile &Sizes"), "view/showTileSizes",
                  &MapViewport::setShowTileSizes);

    viewMenu->addSeparator();

    addViewToggle(tr("Show B&ounds Box"), "view/showBoundsBox",
                  &MapViewport::setShowBounds);
    addViewToggle(tr("Show &Center Point"), "view/showCenterPoint",
                  &MapViewport::setShowCenter);

    viewMenu->addSeparator();

    m_zoomToBoundsAction = viewMenu->addAction(tr("&Zoom to Tileset Bounds"));
    m_zoomToBoundsAction->setEnabled(false);
    connect(m_zoomToBoundsAction, &QAction::triggered, this, &MainWindow::zoomToTilesetBounds);

    viewMenu->addSeparator();

    m_tileScaleMenu = viewMenu->addMenu(tr("Tile &Scale"));
    m_tileScaleMenu->setEnabled(false);
    m_tileScaleGroup = new QActionGroup(this);
    m_tileScaleGroup->setExclusive(true);
    connect(m_tileScaleGroup, &QActionGroup::triggered, this, &MainWindow::onTileScaleChanged);

    // Help menu
    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    auto* aboutAction = helpMenu->addAction(tr("&About TilePeek\u2026"));
    connect(aboutAction, &QAction::triggered, this, [this] {
        AboutDialog dlg(this);
        dlg.exec();
    });
}

void MainWindow::setupToolBar()
{
    //: Name of the main toolbar shown in the toolbar visibility menu.
    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);

    auto* openAction = toolbar->addAction(
        QIcon::fromTheme("document-open"), tr("Open File..."));
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    toolbar->addSeparator();

    m_tileFocusAction = toolbar->addAction(
        QIcon::fromTheme("crosshairs"), tr("Focus Tile"));
    m_tileFocusAction->setCheckable(true);
    m_tileFocusAction->setEnabled(false);
    m_tileFocusAction->setVisible(false);
    connect(m_tileFocusAction, &QAction::toggled, this, [this](bool checked) {
        if (checked && m_mapViewport->isTileFocusActive()) {
            m_mapViewport->exitTileFocus();
        }
        m_mapViewport->setTileFocusSelecting(checked);
    });
    connect(m_mapViewport, &MapViewport::tileFocusChanged, this, [this](bool active) {
        m_tileFocusAction->setChecked(false);
        m_zoomSlider->setEnabled(!active && m_tileProvider != nullptr);
    });

    auto* zoomToBoundsToolbarAction = toolbar->addAction(
        QIcon::fromTheme("zoom-fit-best"), tr("Zoom to Tileset Bounds"));
    zoomToBoundsToolbarAction->setEnabled(false);
    connect(zoomToBoundsToolbarAction, &QAction::triggered, this, &MainWindow::zoomToTilesetBounds);
    // Keep in sync with the menu action's enabled state
    connect(m_zoomToBoundsAction, &QAction::enabledChanged,
            zoomToBoundsToolbarAction, &QAction::setEnabled);

    // Flexible spacer pushes zoom controls to the right
    auto* spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    m_zoomOutAction = toolbar->addAction(
        QIcon::fromTheme("zoom-out"), tr("Zoom Out"));
    m_zoomOutAction->setEnabled(false);
    connect(m_zoomOutAction, &QAction::triggered, m_mapViewport, &MapViewport::zoomOut);

    m_zoomSlider = new QSlider(Qt::Horizontal, toolbar);
    m_zoomSlider->setTickPosition(QSlider::TicksBelow);
    m_zoomSlider->setTickInterval(1);
    m_zoomSlider->setSingleStep(1);
    m_zoomSlider->setPageStep(1);
    m_zoomSlider->setRange(0, 0);
    m_zoomSlider->setEnabled(false);
    m_zoomSlider->setFixedWidth(180);
    toolbar->addWidget(m_zoomSlider);

    auto updateZoomTooltip = [this](int value) {
        m_zoomSlider->setToolTip(tr("Zoom level %1").arg(value));
    };
    connect(m_zoomSlider, &QSlider::valueChanged, m_mapViewport, &MapViewport::setZoom);
    connect(m_zoomSlider, &QSlider::valueChanged, this, updateZoomTooltip);
    connect(m_mapViewport, &MapViewport::zoomChanged, m_zoomSlider, &QSlider::setValue);
    connect(m_mapViewport, &MapViewport::zoomChanged, this, updateZoomTooltip);

    m_zoomInAction = toolbar->addAction(
        QIcon::fromTheme("zoom-in"), tr("Zoom In"));
    m_zoomInAction->setEnabled(false);
    connect(m_zoomInAction, &QAction::triggered, m_mapViewport, &MapViewport::zoomIn);
}

void MainWindow::setupCentralWidget()
{
    m_stack = new QStackedWidget(this);

    m_emptyState = new EmptyStateWidget(m_stack);

    m_splitter = new QSplitter(Qt::Horizontal, m_stack);
    m_sidebar = new MetadataSidebar(m_splitter);
    m_mapViewport = new MapViewport(m_splitter);
    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_mapViewport);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    QSettings settings;
    int sidebarWidth = settings.value("window/sidebarWidth", 360).toInt();
    m_splitter->setSizes({sidebarWidth, std::max(1, width() - sidebarWidth)});

    m_stack->addWidget(m_emptyState);
    m_stack->addWidget(m_splitter);
    m_stack->setCurrentWidget(m_emptyState);

    setCentralWidget(m_stack);
}

void MainWindow::onOpenFile()
{
    //: File dialog filter. Preserve the "*.mbtiles"/"*.pmtiles"/"*" glob
    //: patterns and the ";;" separators exactly; translate only the
    //: descriptive labels in parentheses.
    const QString filter = tr(
        "Tile Archives (*.mbtiles *.pmtiles);;MBTiles Files (*.mbtiles);;PMTiles Files (*.pmtiles);;All Files (*)");
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Tile Archive"), QString(), filter);
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
        QMessageBox::warning(this, tr("Error"), tr("Failed to open file: %1").arg(path));
        return;
    }

    auto validation = reader->validateSchema();
    if (!validation.metadataTableValid || !validation.tilesTableValid) {
        QMessageBox::warning(this, tr("Error"), validation.errors.join("\n"));
        return;
    }

    auto rawMetadata = reader->readRawMetadata();
    auto zoomRange = reader->queryZoomRange();

    auto [metadata, messages] = MBTilesMetadataParser::parse(rawMetadata, zoomRange);

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

    // Compute tileset bounds: prefer metadata, fall back to tile grid at minZoom
    auto boundsOpt = MBTilesMetadataParser::parseBounds(metadata.value("bounds").value_or(""));
    if (!boundsOpt) {
        auto grid = reader->queryTileGridBounds(minZoom);
        if (grid) {
            double left = static_cast<double>(grid->minX) / (1 << minZoom) * 360.0 - 180.0;
            double right = static_cast<double>(grid->maxX + 1) / (1 << minZoom) * 360.0 - 180.0;
            double top = WebMercator::pixelYToLat(
                static_cast<double>(grid->minY) * WebMercator::TileSize, minZoom);
            double bottom = WebMercator::pixelYToLat(
                static_cast<double>(grid->maxY + 1) * WebMercator::TileSize, minZoom);
            boundsOpt = ParsedBounds{left, bottom, right, top};
        }
    }
    m_tilesetBounds = boundsOpt;

    if (format == "pbf") {
        // Vector tile path
        QStringList layerNames;
        auto jsonStr = metadata.value("json");
        if (!jsonStr) {
            messages.append({ValidationMessage::Level::Error,
                             tr("Missing required 'json' metadata key for pbf format"), "json"});
            m_sidebar->setMetadata(metadata, messages);
        } else {
            auto vResult = VectorMetadataParser::parse(*jsonStr);
            messages.append(vResult.messages);
            if (vResult.metadata) {
                for (const auto& layer : vResult.metadata->vectorLayers)
                    layerNames << layer.id;
                auto colors = CetColormap::pickColors(layerNames.size());
                QList<QColor> colorList(colors.begin(), colors.end());
                m_sidebar->setVectorMetadata(metadata, *vResult.metadata, colorList, messages);
            } else {
                m_sidebar->setMetadata(metadata, messages);
            }
        }

        auto vectorProvider = std::make_unique<VectorTileProvider>(
            std::move(reader), minZoom, maxZoom, layerNames);
        vectorProvider->setRenderSize(512);
        m_tileProvider = std::move(vectorProvider);
        m_mapViewport->setBackgroundColor(QColor("#202122"));
        m_mapViewport->setVectorProvider(true);
        m_mapViewport->setDisplayTileSize(512);
        populateTileScaleMenu(true);

        connect(m_sidebar, &MetadataSidebar::layerVisibilityChanged,
                this, &MainWindow::onLayerVisibilityChanged);
        connect(m_sidebar, &MetadataSidebar::labeledFieldsChanged,
                this, &MainWindow::onLabeledFieldsChanged);
        connect(m_mapViewport, &MapViewport::inspectRequested,
                this, &MainWindow::onInspectRequested);
        connect(m_mapViewport, &MapViewport::inspectCleared,
                this, &MainWindow::onInspectCleared);
        connect(m_sidebar, &MetadataSidebar::featureIsolated,
                m_mapViewport, &MapViewport::isolateInspectHighlight);
    } else {
        // Raster tile path
        auto provider = std::make_unique<RasterTileProvider>(
            std::move(reader), format, minZoom, maxZoom);

        auto formatResult = provider->validateFormat();
        switch (formatResult.status) {
        case FormatValidationResult::Status::Unsupported:
            QMessageBox::warning(this, tr("Error"), formatResult.message);
            return;
        case FormatValidationResult::Status::UnrecognizedFormat:
            QMessageBox::warning(this, tr("Error"), formatResult.message);
            return;
        case FormatValidationResult::Status::FormatMismatch:
            messages.append({ValidationMessage::Level::Warning, formatResult.message, "format"});
            break;
        case FormatValidationResult::Status::Ok:
            break;
        }

        m_sidebar->setMetadata(metadata, messages);
        m_nativeTileSize = provider->detectNativeTileSize();
        m_mapViewport->setDisplayTileSize(m_nativeTileSize);
        m_tileProvider = std::move(provider);
        populateTileScaleMenu(false);
    }

    m_mapViewport->setTileProvider(m_tileProvider);
    m_stack->setCurrentIndex(1);
    m_zoomInAction->setEnabled(true);
    m_zoomOutAction->setEnabled(true);
    m_zoomSlider->setRange(minZoom, maxZoom);
    m_zoomSlider->setEnabled(true);
    m_tileFocusAction->setEnabled(m_isVectorFormat);
    m_tileFocusAction->setVisible(m_isVectorFormat);

    // Pass metadata bounds and center to viewport for overlay display
    auto metadataBounds = MBTilesMetadataParser::parseBounds(metadata.value("bounds").value_or(""));
    auto centerOpt = MBTilesMetadataParser::parseCenter(metadata.value("center").value_or(""));

    m_mapViewport->setBounds(metadataBounds);
    m_mapViewport->setCenter(centerOpt);
    m_zoomToBoundsAction->setEnabled(m_tilesetBounds.has_value());

    if (centerOpt) {
        m_mapViewport->setView(centerOpt->longitude, centerOpt->latitude, minZoom);
    } else {
        m_mapViewport->setView(0.0, 0.0, minZoom);
    }

    setWindowTitle(tr("TilePeek - %1").arg(QFileInfo(path).fileName()));

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
        QMessageBox::warning(this, tr("Error"), tr("Failed to open file: %1").arg(path));
        return;
    }

    auto validation = reader->validate();
    if (!validation.valid) {
        QMessageBox::warning(this, tr("Error"), validation.errors.join("\n"));
        return;
    }

    const auto& header = reader->header();
    auto jsonMeta = reader->readJsonMetadata();
    auto [metadata, messages] = PMTilesMetadataParser::parse(header, jsonMeta);

    QString format = PMTilesMetadataParser::tileTypeToFormat(header.tile_type);
    int minZoom = header.min_zoom;
    int maxZoom = header.max_zoom;

    // Compute tileset bounds: prefer metadata, fall back to tile grid at minZoom
    {
        auto metaBounds = MBTilesMetadataParser::parseBounds(metadata.value("bounds").value_or(""));
        if (metaBounds) {
            m_tilesetBounds = metaBounds;
        } else {
            auto grid = reader->queryTileGridBounds(minZoom);
            if (grid) {
                double left = static_cast<double>(grid->minX) / (1 << minZoom) * 360.0 - 180.0;
                double right = static_cast<double>(grid->maxX + 1) / (1 << minZoom) * 360.0 - 180.0;
                double top = WebMercator::pixelYToLat(
                    static_cast<double>(grid->minY) * WebMercator::TileSize, minZoom);
                double bottom = WebMercator::pixelYToLat(
                    static_cast<double>(grid->maxY + 1) * WebMercator::TileSize, minZoom);
                m_tilesetBounds = ParsedBounds{left, bottom, right, top};
            } else {
                m_tilesetBounds.reset();
            }
        }
    }

    if (format == "pbf") {
        // Vector tile path
        QStringList layerNames;
        auto jsonStr = metadata.value("json");
        if (!jsonStr) {
            if (!jsonMeta.isEmpty())
                messages.append({ValidationMessage::Level::Warning,
                                 tr("No vector_layers found in PMTiles metadata")});
            m_sidebar->setMetadata(metadata, messages);
        } else {
            auto vResult = VectorMetadataParser::parse(*jsonStr);
            messages.append(vResult.messages);
            if (vResult.metadata) {
                for (const auto& layer : vResult.metadata->vectorLayers)
                    layerNames << layer.id;
                auto colors = CetColormap::pickColors(layerNames.size());
                QList<QColor> colorList(colors.begin(), colors.end());
                m_sidebar->setVectorMetadata(metadata, *vResult.metadata, colorList, messages);
            } else {
                m_sidebar->setMetadata(metadata, messages);
            }
        }

        auto vectorProvider = std::make_unique<VectorTileProvider>(
            std::move(reader), minZoom, maxZoom, layerNames);
        vectorProvider->setRenderSize(512);
        m_tileProvider = std::move(vectorProvider);
        m_mapViewport->setBackgroundColor(QColor("#202122"));
        m_mapViewport->setVectorProvider(true);
        m_mapViewport->setDisplayTileSize(512);
        populateTileScaleMenu(true);

        connect(m_sidebar, &MetadataSidebar::layerVisibilityChanged,
                this, &MainWindow::onLayerVisibilityChanged);
        connect(m_sidebar, &MetadataSidebar::labeledFieldsChanged,
                this, &MainWindow::onLabeledFieldsChanged);
        connect(m_mapViewport, &MapViewport::inspectRequested,
                this, &MainWindow::onInspectRequested);
        connect(m_mapViewport, &MapViewport::inspectCleared,
                this, &MainWindow::onInspectCleared);
        connect(m_sidebar, &MetadataSidebar::featureIsolated,
                m_mapViewport, &MapViewport::isolateInspectHighlight);
    } else {
        // Raster tile path
        auto provider = std::make_unique<RasterTileProvider>(
            std::move(reader), format, minZoom, maxZoom);

        m_sidebar->setMetadata(metadata, messages);
        m_nativeTileSize = provider->detectNativeTileSize();
        m_mapViewport->setDisplayTileSize(m_nativeTileSize);
        m_tileProvider = std::move(provider);
        populateTileScaleMenu(false);
    }

    m_mapViewport->setTileProvider(m_tileProvider);
    m_stack->setCurrentIndex(1);
    m_zoomInAction->setEnabled(true);
    m_zoomOutAction->setEnabled(true);
    m_zoomSlider->setRange(minZoom, maxZoom);
    m_zoomSlider->setEnabled(true);
    m_tileFocusAction->setEnabled(m_isVectorFormat);
    m_tileFocusAction->setVisible(m_isVectorFormat);

    // Pass metadata bounds and center to viewport for overlay display
    auto metadataBounds = MBTilesMetadataParser::parseBounds(metadata.value("bounds").value_or(""));
    auto centerOpt = MBTilesMetadataParser::parseCenter(metadata.value("center").value_or(""));

    m_mapViewport->setBounds(metadataBounds);
    m_mapViewport->setCenter(centerOpt);
    m_zoomToBoundsAction->setEnabled(m_tilesetBounds.has_value());

    if (centerOpt) {
        m_mapViewport->setView(centerOpt->longitude, centerOpt->latitude, minZoom);
    } else {
        m_mapViewport->setView(0.0, 0.0, minZoom);
    }

    setWindowTitle(tr("TilePeek - %1").arg(QFileInfo(path).fileName()));

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
        // Determine which layers changed visibility
        QSet<QString> newlyHidden = hiddenLayers - vtp->hiddenLayers();

        vtp->setHiddenLayers(hiddenLayers);
        m_mapViewport->invalidateTiles();

        // Remove highlights for newly hidden layers
        if (!newlyHidden.isEmpty())
            m_mapViewport->removeInspectHighlightsForLayers(newlyHidden);
    }
}

void MainWindow::onLabeledFieldsChanged(const QHash<QString, QString>& fieldsByLayer)
{
    if (auto* vtp = dynamic_cast<VectorTileProvider*>(m_tileProvider.get())) {
        vtp->setLabeledFields(fieldsByLayer);
        m_mapViewport->invalidateTiles();
    }
}

void MainWindow::onInspectRequested(TileKey tile, QPointF tileLocalPos, double tileSize,
                                     double scale)
{
    auto* vtp = dynamic_cast<VectorTileProvider*>(m_tileProvider.get());
    if (!vtp)
        return;

    auto decodedTile = vtp->decodeTileAt(tile.zoom, tile.x, tile.y);
    if (!decodedTile)
        return;

    // Convert screen-pixel radii to tile-local units
    double hitRadius = 2.0 / scale;
    double pointRadius = 3.0 / scale; // matches rendered dot radius

    auto results = mvt::hitTest(*decodedTile, tileLocalPos, tileSize, hitRadius, pointRadius,
                                vtp->hiddenLayers(), vtp->layerColors());

    if (results.isEmpty()) {
        // Empty click: clear highlights and sidebar
        m_mapViewport->clearInspectHighlights();
        m_sidebar->clearInspectResults();
        return;
    }

    m_sidebar->setInspectResults(results);

    // Build highlight geometry for each hit feature
    QList<mvt::FeatureHighlight> highlights;
    for (const auto& r : results) {
        const auto& layer = decodedTile->layers[r.layerIndex];
        const auto& feature = layer.features[r.featureIndex];

        mvt::FeatureHighlight h;
        h.color = r.layerColor;
        h.type = r.geomType;
        h.layerName = r.layerName;

        if (r.geomType == mvt::GeomType::Point) {
            h.points = mvt::decodePoints(feature, layer.extent, tileSize);
        } else {
            h.path = mvt::decodeGeometry(feature, layer.extent, tileSize);
        }
        highlights.append(h);
    }

    m_mapViewport->setInspectHighlights(tile, tileSize, highlights);
}

void MainWindow::zoomToTilesetBounds()
{
    if (m_tilesetBounds)
        m_mapViewport->zoomToBounds(m_tilesetBounds->left, m_tilesetBounds->bottom,
                                     m_tilesetBounds->right, m_tilesetBounds->top);
}

void MainWindow::onInspectCleared()
{
    m_sidebar->clearInspectResults();
}

void MainWindow::populateTileScaleMenu(bool isVector)
{
    // Clear existing actions
    for (auto* action : m_tileScaleGroup->actions())
        m_tileScaleGroup->removeAction(action);
    m_tileScaleMenu->clear();

    m_isVectorFormat = isVector;

    if (isVector) {
        struct Preset { QString label; int size; };
        const Preset presets[] = {
            {tr("256px"), 256}, {tr("512px"), 512}, {tr("1024px"), 1024}};
        for (const auto& [label, size] : presets) {
            auto* action = m_tileScaleMenu->addAction(label);
            action->setCheckable(true);
            action->setData(size);
            m_tileScaleGroup->addAction(action);
            if (size == 512)
                action->setChecked(true);
        }
    } else {
        struct Preset { QString label; int factor; };
        const Preset presets[] = {
            {tr("1\u00d7 (Native)"), 1}, {tr("2\u00d7"), 2}, {tr("3\u00d7"), 3}};
        for (const auto& [label, factor] : presets) {
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
    disconnect(m_sidebar, &MetadataSidebar::labeledFieldsChanged,
               this, &MainWindow::onLabeledFieldsChanged);
    disconnect(m_mapViewport, &MapViewport::inspectRequested,
               this, &MainWindow::onInspectRequested);
    disconnect(m_mapViewport, &MapViewport::inspectCleared,
               this, &MainWindow::onInspectCleared);
    disconnect(m_sidebar, &MetadataSidebar::featureIsolated,
               m_mapViewport, &MapViewport::isolateInspectHighlight);
    m_mapViewport->clear();
    m_mapViewport->setBackgroundColor(palette().color(QPalette::Window));
    m_tileProvider.reset();
    m_sidebar->clear();
    m_stack->setCurrentWidget(m_emptyState);
    m_zoomInAction->setEnabled(false);
    m_zoomOutAction->setEnabled(false);
    m_zoomSlider->setRange(0, 0);
    m_zoomSlider->setEnabled(false);
    m_tileFocusAction->setEnabled(false);
    m_tileFocusAction->setVisible(false);
    m_tileFocusAction->setChecked(false);
    m_tileScaleMenu->setEnabled(false);
    for (auto* action : m_tileScaleGroup->actions())
        m_tileScaleGroup->removeAction(action);
    m_tileScaleMenu->clear();
    m_zoomToBoundsAction->setEnabled(false);
    m_tilesetBounds.reset();
    m_nativeTileSize = 256;
    m_isVectorFormat = false;
    setWindowTitle(tr("TilePeek"));
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

void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    if (!isMaximized())
        settings.setValue("window/size", size());
    settings.setValue("window/sidebarWidth", m_splitter->sizes().value(0));
    QMainWindow::closeEvent(event);
}
