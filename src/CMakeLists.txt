find_package(ZLIB REQUIRED)

configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

qt_add_resources(TILEPEEK_RESOURCES_ICONS ${CMAKE_SOURCE_DIR}/resources/icons/icons.qrc)
qt_add_resources(TILEPEEK_RESOURCES_GRAPHICS ${CMAKE_SOURCE_DIR}/resources/graphics/graphics.qrc)
qt_add_resources(TILEPEEK_RESOURCES_TEXT ${CMAKE_SOURCE_DIR}/resources/text/text.qrc)

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
    mvt/FeatureHitTest.cpp
    mvt/MvtDecoder.cpp
    mvt/MvtGeometry.cpp
    mvt/ProtobufDecoder.cpp
    pmtiles/PMTilesReader.cpp
    pmtiles/PMTilesMetadataParser.cpp
    stats/TileStatsWorker.cpp
    widgets/MetadataSidebar.cpp
    widgets/EmptyStateWidget.cpp
    util/FormatUtils.cpp
    util/GzipUtils.cpp
    util/TileCoords.cpp
)
target_include_directories(tilepeek_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/vendor
)
target_link_libraries(tilepeek_lib PUBLIC Qt6::Widgets Qt6::Sql Qt6::Svg Qt6::Concurrent ZLIB::ZLIB)

find_program(RSVG_CONVERT rsvg-convert)

if(APPLE)
    find_program(ICONUTIL iconutil)
    if(NOT RSVG_CONVERT OR NOT ICONUTIL)
        message(FATAL_ERROR "macOS builds require rsvg-convert (brew install librsvg) and iconutil")
    endif()

    set(TILEPEEK_ICON_NAME "tilepeek.icns")
    set(TILEPEEK_ICNS_PATH ${CMAKE_CURRENT_BINARY_DIR}/${TILEPEEK_ICON_NAME})
    set(ICONSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/tilepeek.iconset)
    set(ICONSET_SVG ${CMAKE_SOURCE_DIR}/resources/icons/tilepeek.svg)

    # Apple's .iconset naming convention. Each file's pixel size is the logical
    # size times the @Nx scale factor. iconutil -c icns packs these into .icns.
    set(ICONSET_ENTRIES
        "16 1 16x16"
        "32 2 16x16@2x"
        "32 1 32x32"
        "64 2 32x32@2x"
        "128 1 128x128"
        "256 2 128x128@2x"
        "256 1 256x256"
        "512 2 256x256@2x"
        "512 1 512x512"
        "1024 2 512x512@2x")

    set(ICONSET_PNGS "")
    foreach(ENTRY ${ICONSET_ENTRIES})
        string(REPLACE " " ";" PARTS ${ENTRY})
        list(GET PARTS 0 PX)
        list(GET PARTS 2 LABEL)
        set(PNG ${ICONSET_DIR}/icon_${LABEL}.png)
        add_custom_command(
            OUTPUT ${PNG}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ICONSET_DIR}
            COMMAND ${RSVG_CONVERT} -w ${PX} -h ${PX} ${ICONSET_SVG} -o ${PNG}
            DEPENDS ${ICONSET_SVG}
            COMMENT "Rendering icon ${LABEL} (${PX}px)")
        list(APPEND ICONSET_PNGS ${PNG})
    endforeach()

    add_custom_command(
        OUTPUT ${TILEPEEK_ICNS_PATH}
        COMMAND ${ICONUTIL} -c icns -o ${TILEPEEK_ICNS_PATH} ${ICONSET_DIR}
        DEPENDS ${ICONSET_PNGS}
        COMMENT "Packing tilepeek.icns")

    set_source_files_properties(${TILEPEEK_ICNS_PATH} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
        GENERATED TRUE)

    add_executable(tilepeek MACOSX_BUNDLE
        main.cpp
        ${TILEPEEK_RESOURCES_ICONS}
        ${TILEPEEK_RESOURCES_GRAPHICS}
        ${TILEPEEK_RESOURCES_TEXT}
        ${TILEPEEK_ICNS_PATH})

    set_target_properties(tilepeek PROPERTIES
        OUTPUT_NAME "TilePeek"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/resources/macos/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.tilepeek.TilePeek"
        MACOSX_BUNDLE_BUNDLE_NAME "TilePeek"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "${TILEPEEK_ICON_NAME}"
        MACOSX_BUNDLE_COPYRIGHT "© 2026 AJ Ashton")

    target_link_libraries(tilepeek PRIVATE tilepeek_lib)
else()
    add_executable(tilepeek main.cpp ${TILEPEEK_RESOURCES_ICONS} ${TILEPEEK_RESOURCES_GRAPHICS} ${TILEPEEK_RESOURCES_TEXT})
    target_link_libraries(tilepeek PRIVATE tilepeek_lib)

    # Install targets (Linux / XDG)
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

    # Generate PNG icons from SVG at build time (for hicolor theme)
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
endif()
