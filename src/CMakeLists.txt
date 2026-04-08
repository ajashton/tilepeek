add_library(tilepeek_lib STATIC
    app/MainWindow.cpp
    mbtiles/MBTilesReader.cpp
    mbtiles/MBTilesMetadataParser.cpp
    model/TilesetMetadata.cpp
    widgets/MetadataSidebar.cpp
    widgets/ToastManager.cpp
    widgets/ToastWidget.cpp
    util/TileCoords.cpp
)
target_include_directories(tilepeek_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tilepeek_lib PUBLIC Qt6::Widgets Qt6::Sql)

add_executable(tilepeek main.cpp)
target_link_libraries(tilepeek PRIVATE tilepeek_lib)
