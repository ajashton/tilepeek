find_package(ZLIB REQUIRED)

configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

qt_add_resources(TILEPEEK_RESOURCES ${CMAKE_SOURCE_DIR}/resources/icons/icons.qrc)

add_library(tilepeek_lib STATIC
    app/AboutDialog.cpp
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
    pmtiles/PMTilesReader.cpp
    pmtiles/PMTilesMetadataParser.cpp
    stats/TileStatsWorker.cpp
    widgets/MetadataSidebar.cpp
    widgets/ToastManager.cpp
    widgets/ToastWidget.cpp
    util/FormatUtils.cpp
    util/GzipUtils.cpp
    util/TileCoords.cpp
    ${TILEPEEK_RESOURCES}
)
target_include_directories(tilepeek_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/vendor
)
target_link_libraries(tilepeek_lib PUBLIC Qt6::Widgets Qt6::Sql ZLIB::ZLIB)

add_executable(tilepeek main.cpp)
target_link_libraries(tilepeek PRIVATE tilepeek_lib)

# Install targets
install(TARGETS tilepeek RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_SOURCE_DIR}/data/com.tilepeek.TilePeek.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_SOURCE_DIR}/data/com.tilepeek.TilePeek.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES ${CMAKE_SOURCE_DIR}/data/com.tilepeek.TilePeek.mime.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/tilepeek.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME com.tilepeek.TilePeek.svg)

# Generate PNG icons from SVG at build time
find_program(RSVG_CONVERT rsvg-convert)
if(RSVG_CONVERT)
    set(ICON_SIZES 16 24 32 48 64 128 256)
    set(ICON_FILES "")
    foreach(SIZE ${ICON_SIZES})
        set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/icons/${SIZE}x${SIZE})
        add_custom_command(
            OUTPUT ${OUTPUT_DIR}/com.tilepeek.TilePeek.png
            COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
            COMMAND ${RSVG_CONVERT} -w ${SIZE} -h ${SIZE}
                ${CMAKE_SOURCE_DIR}/resources/icons/tilepeek.svg
                -o ${OUTPUT_DIR}/com.tilepeek.TilePeek.png
            DEPENDS ${CMAKE_SOURCE_DIR}/resources/icons/tilepeek.svg
            COMMENT "Generating ${SIZE}x${SIZE} icon"
        )
        list(APPEND ICON_FILES ${OUTPUT_DIR}/com.tilepeek.TilePeek.png)
        install(FILES ${OUTPUT_DIR}/com.tilepeek.TilePeek.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/apps)
    endforeach()
    add_custom_target(icons ALL DEPENDS ${ICON_FILES})
endif()
