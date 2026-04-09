find_package(ZLIB REQUIRED)

add_library(tilepeek_lib STATIC
    app/MainWindow.cpp
    map/MapViewport.cpp
    map/RasterTileProvider.cpp
    map/TileCache.cpp
    map/VectorTileProvider.cpp
    map/VectorTileRenderer.cpp
    map/WebMercator.cpp
    mbtiles/MBTilesReader.cpp
    mbtiles/MBTilesMetadataParser.cpp
    mbtiles/VectorMetadataParser.cpp
    model/TilesetMetadata.cpp
    mvt/MvtDecoder.cpp
    mvt/MvtGeometry.cpp
    mvt/ProtobufDecoder.cpp
    stats/TileStatsWorker.cpp
    widgets/MetadataSidebar.cpp
    widgets/ToastManager.cpp
    widgets/ToastWidget.cpp
    util/FormatUtils.cpp
    util/GzipUtils.cpp
    util/TileCoords.cpp
)
target_include_directories(tilepeek_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tilepeek_lib PUBLIC Qt6::Widgets Qt6::Sql ZLIB::ZLIB)

add_executable(tilepeek main.cpp)
target_link_libraries(tilepeek PRIVATE tilepeek_lib)
